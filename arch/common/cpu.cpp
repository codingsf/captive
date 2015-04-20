#include <cpu.h>
#include <barrier.h>
#include <mmu.h>
#include <decode.h>
#include <interp.h>
#include <disasm.h>
#include <string.h>
#include <safepoint.h>
#include <priv.h>
#include <jit.h>
#include <jit/translation-context.h>
#include <profile/image.h>

using namespace captive::arch;
using namespace captive::arch::jit;

safepoint_t cpu_safepoint;

CPU *CPU::current_cpu;

CPU::CPU(Environment& env, profile::Image& profile_image, PerCPUData *per_cpu_data) : _env(env), _per_cpu_data(per_cpu_data), _exec_txl(false), _profile_image(profile_image)
{
	// Zero out the local state.
	bzero(&local_state, sizeof(local_state));

	// Initialise the decode cache
	memset(decode_cache, 0xff, sizeof(decode_cache));

	// Initialise the block cache
	memset(block_cache, 0xff, sizeof(block_cache));

	jit_state.cpu = this;
	jit_state.region_chaining_table = (void **)malloc(sizeof(void *) * 0x100000);
}

CPU::~CPU()
{
	free(jit_state.region_chaining_table);
}

bool CPU::handle_pending_action(uint32_t action)
{
	switch (action) {
	case 2:
		dump_state();
		return true;

	case 3:
		trace().enable();
		return true;

	case 4:
		asm volatile("out %0, $0xff\n" :: "a"(4));
		return true;
	}

	return false;
}


bool CPU::run()
{
	switch (_per_cpu_data->execution_mode) {
	case 0:
		return run_interp();
	case 1:
		return run_block_jit();
	case 2:
		return run_region_jit();
	default:
		return false;
	}
}

bool CPU::check_safepoint()
{
	// Create a safepoint for returning from a memory access fault
	int rc = record_safepoint(&cpu_safepoint);
	if (rc > 0) {
		// Reset the executing translation flag.
		_exec_txl = false;

		// If the return code is greater than zero, then we returned
		// from a fault.

		// If we're tracing, add a descriptive message, and close the
		// trace packet.
		if (unlikely(trace().enabled())) {
			trace().add_str("memory exception taken");
			trace().end_record();
		}

		// Instruct the interpreter to handle the memory fault, passing
		// in the the type of fault.
		interpreter().handle_memory_fault((MMU::resolution_fault)rc);

		// Make sure interrupts are enabled.
		__local_irq_enable();
	}

	// Make sure we're operating in the correct mode
	ensure_privilege_mode();

	return true;
}

bool CPU::run_interp()
{
	bool step_ok = true;

	printf("cpu: starting interpretive cpu execution\n");

	if (!check_safepoint()) {
		return false;
	}

	do {
		// Check the ISR to determine if there is an interrupt pending,
		// and if there is, instruct the interpreter to handle it.
		if (unlikely(cpu_data().isr)) {
			interpreter().handle_irq(cpu_data().isr);
		}

		// Check to see if there are any pending actions coming in from
		// the hypervisor.
		if (unlikely(cpu_data().async_action)) {
			if (handle_pending_action(cpu_data().async_action)) {
				cpu_data().async_action = 0;
			}
		}

		step_ok = interpret_block();
	} while(step_ok);

	return true;
}

bool CPU::run_block_jit()
{
	bool step_ok = true;

	printf("cpu: starting block-jit cpu execution\n");

	return false;
}

bool CPU::interpret_block()
{
	bool step_ok = false;

	// Now, execute one basic-block of instructions.
	Decode *insn;
	do {
		// Get the address of the next instruction to execute
		uint32_t pc = read_pc();

		assert_privilege_mode();

		// Obtain a decode object for this PC, and perform the decode.
		insn = get_decode(pc);
		if (1) { //insn->pc != pc) {
			if (unlikely(!decode_instruction_virt(pc, insn))) {
				printf("cpu: unhandled decode fault @ %08x\n", pc);
				return false;
			}
		}

		// Perhaps trace this instruction
		if (unlikely(trace().enabled())) {
			trace().start_record(get_insns_executed(), pc, insn);
		}

		// Perhaps verify this instruction
		if (unlikely(cpu_data().verify_enabled) && !verify_check()) {
			return false;
		}

		// Execute the instruction, with interrupts disabled.
		__local_irq_disable();
		step_ok = interpreter().step_single(insn);
		__local_irq_enable();

		// Increment the instruction counter.
		inc_insns_executed();

		// Perhaps finish tracing this instruction.
		if (unlikely(trace().enabled())) {
			trace().end_record();
		}
	} while(!insn->end_of_block && step_ok);
}

void CPU::invalidate_executed_page(pa_t phys_page_base_addr, va_t virt_page_base_addr)
{
	if (virt_page_base_addr > (va_t)0x100000000) return;

	profile_image().invalidate((gpa_t)((uint64_t)phys_page_base_addr & 0xffffffffULL));

	// Evict entries from the decode cache and block cache
	/*uint32_t page_base = (uint32_t)(uint64_t)page_base_addr;
	for (uint32_t pc = page_base; pc < page_base + 0x1000; pc += 4) {
		Decode *decode = get_decode(pc);

		if (decode->pc == pc) {
			decode->pc = 0xffffffff;
		}

		GuestBasicBlock *block = get_block(pc);

		if (block->block_address() == pc) {
			if (block->valid()) {
				block->release_memory();
			}

			block->invalidate();
		}
	}*/
}

bool CPU::verify_check()
{
	Barrier *enter = (Barrier *)cpu_data().verify_data->barrier_enter;
	Barrier *exit = (Barrier *)cpu_data().verify_data->barrier_exit;

	// If we're not the primary CPU, copy our state into the shared memory region.
	if (cpu_data().verify_tid > 0) {
		memcpy((void *)cpu_data().verify_data->cpu_data, (const void *)reg_state(), reg_state_size());
	}

	// Wait on the entry barrier
	enter->wait(cpu_data().verify_tid);

	// Tick!
	asm volatile("out %0, $0xff" :: "a"(9));

	// If we're the primary CPU, perform verification.
	if (cpu_data().verify_tid == 0) {
		// Compare OUR register state to the other CPU's.
		if (memcmp((const void *)reg_state(), (const void *)cpu_data().verify_data->cpu_data, reg_state_size())) {
			cpu_data().verify_data->verify_failed = true;
		} else {
			cpu_data().verify_data->verify_failed = false;
		}

	}

	// Wait on the exit barrier
	exit->wait(cpu_data().verify_tid);

	// Check to see if we failed, and if so, dump our state and return a failure code.
	if (cpu_data().verify_data->verify_failed) {
		printf("*** DIVERGENCE DETECTED ***\n");
		dump_state();
		return false;
	} else {
		return true;
	}
}
