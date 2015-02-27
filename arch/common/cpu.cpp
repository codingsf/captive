#include <cpu.h>
#include <mmu.h>

using namespace captive::arch;

CPU *captive::arch::active_cpu;

CPU::CPU(Environment& env) : _env(env), insns_executed(0)
{
	bzero(decode_cache, sizeof(decode_cache));
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

	printf("starting cpu execution\n");

	int rc = record_safepoint(&cpu_safepoint);
	if (rc > 0) {
		if (trace().enabled()) {
			trace().add_str("memory exception taken");
			trace().end_record();
		}

		interpreter().handle_memory_fault((MMU::resolution_fault)rc);

		/*if (rc == MMU::READ_FAULT || rc == MMU::WRITE_FAULT) {
			interpreter().ha take_exception(7, state.regs.RB[15] + 8);
		} else {
			interp.take_exception(6, state.regs.RB[15] + 4);
		}*/

		__local_irq_enable();
	}

	do {
		if (shmem->isr) {
			if (shmem->isr & 1) {
				interpreter().handle_irq(1);
			}

			if (shmem->isr & 2) {
				interpreter().handle_irq(0);
			}
		}

		// Check pending actions
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

		// Execute one block of instructions
		Decode *insn;
		do {
			// Reset CPU exception state
			local_state.last_exception_action = 0;

			// Get the address of the next instruction to execute
			uint32_t pc = read_pc();

			// Obtain a decode object for this PC, and see if it's
			// been cached
			insn = get_decode(pc);
			if (1) { //pc == 0 || insn->pc != pc) {
				if (insn->decode(ArmDecode::arm, pc)) {
					printf("cpu: unhandled decode fault @ %08x\n", pc);
					return false;
				}
			}

			// Perhaps trace this instruction
			if (trace().enabled()) trace().start_record(get_insns_executed(), pc, (const uint8_t *)insn);

			// Execute the instruction
			__local_irq_disable();
			step_ok = interpreter().step_single(*insn);
			__local_irq_enable();

			inc_insns_executed();

			if (trace().enabled()) trace().end_record();
		} while(!insn->end_of_block && step_ok);
	} while(step_ok);

	return true;
}
