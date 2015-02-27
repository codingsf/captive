#include <cpu.h>
#include <mmu.h>
#include <decode.h>
#include <interp.h>
#include <string.h>
#include <safepoint.h>
#include <shmem.h>

using namespace captive::arch;

CPU *captive::arch::active_cpu;
safepoint_t cpu_safepoint;

extern captive::shmem_data *shmem;

CPU::CPU(Environment& env) : _env(env), insns_executed(0)
{
	bzero(decode_cache, sizeof(decode_cache));
	bzero(&local_state, sizeof(local_state));
}

CPU::~CPU()
{

}

bool CPU::run()
{
	return run_interp();
}

bool CPU::run_interp()
{
	bool step_ok = true;

	printf("starting interpretive cpu execution\n");

	// Create a safepoint for returning from a memory access fault
	int rc = record_safepoint(&cpu_safepoint);
	if (rc > 0) {
		// If the return code is greater than zero, then we returned
		// from a fault.

		// If we're tracing, add a descriptive message, and close the
		// trace packet.
		if (trace().enabled()) {
			trace().add_str("memory exception taken");
			trace().end_record();
		}

		// Instruct the interpreter to handle the memory fault, passing
		// in the the type of fault.
		interpreter().handle_memory_fault((MMU::resolution_fault)rc);

		// Make sure interrupts are enabled.
		__local_irq_enable();
	}

	do {
		// Check the ISR to determine if there is an interrupt pending,
		// and if there is, instruct the interpreter to handle it.
		if (shmem->isr) {
			interpreter().handle_irq(shmem->isr);
		}

		// Check to see if there are any pending actions coming in from
		// the hypervisor.
		if (shmem->asynchronous_action_pending) {
			switch (shmem->asynchronous_action_pending) {
			case 2:
				dump_state();
				shmem->asynchronous_action_pending = 0;
				break;

			case 3:
				trace().enable();
				shmem->asynchronous_action_pending = 0;
				break;

			case 4:
				shmem->insn_count = get_insns_executed();
				shmem->asynchronous_action_pending = 0;
				asm volatile("out %0, $0xff\n" :: "a"(4));
				break;
			}
		}

		// Now, execute one basic-block of instructions.
		Decode *insn;
		do {
			// Reset CPU exception state
			local_state.last_exception_action = 0;

			// Get the address of the next instruction to execute
			uint32_t pc = read_pc();

			// Obtain a decode object for this PC, and perform the decode.
			insn = get_decode(pc);
			if (!decode_instruction(pc, insn)) {
				printf("cpu: unhandled decode fault @ %08x\n", pc);
				return false;
			}

			// Perhaps trace this instruction
			if (trace().enabled()) {
				trace().start_record(get_insns_executed(), pc, insn);
			}

			// Execute the instruction, with interrupts disabled.
			__local_irq_disable();
			step_ok = interpreter().step_single(insn);
			__local_irq_enable();

			// Increment the instruction counter.
			inc_insns_executed();

			// Perhaps finish tracing this instruction.
			if (trace().enabled()) {
				trace().end_record();
			}
		} while(!insn->end_of_block && step_ok);
	} while(step_ok);

	return true;
}
