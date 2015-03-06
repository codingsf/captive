#include <cpu.h>
#include <barrier.h>
#include <mmu.h>
#include <decode.h>
#include <interp.h>
#include <disasm.h>
#include <string.h>
#include <safepoint.h>
#include <jit.h>
#include <jit/guest-basic-block.h>
#include <jit/translation-context.h>
#include <shmem.h>

using namespace captive::arch;
using namespace captive::arch::jit;

safepoint_t cpu_safepoint;

extern captive::shmem_data *shmem;

CPU *CPU::current_cpu;

CPU::CPU(Environment& env) : _env(env), insns_executed(0)
{
	bzero(&local_state, sizeof(local_state));

	flush_decode_cache();
}

CPU::~CPU()
{

}

void CPU::flush_decode_cache()
{
	bzero(decode_cache, sizeof(decode_cache));
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
		shmem->insn_count = get_insns_executed();
		asm volatile("out %0, $0xff\n" :: "a"(4));

		return true;
	}

	return false;
}


bool CPU::run(unsigned int mode)
{
	switch (mode) {
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

bool CPU::run_interp()
{
	bool step_ok = true;

	printf("cpu: starting interpretive cpu execution\n");

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
			handle_pending_action(shmem->asynchronous_action_pending);
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

			// Mark the page as has_been_executed
			//mmu().set_page_executed(pc);

			if (!verify_check()) {
				step_ok = false;
				break;
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

bool CPU::run_block_jit()
{
	bool step_ok = true;

	printf("cpu: starting block-jit cpu execution\n");

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
			handle_pending_action(shmem->asynchronous_action_pending);
		}

		jit::GuestBasicBlock *block = get_basic_block(read_pc());
		if (!block) {
			printf("jit: unable to get basic block @ %08x\n", read_pc());
			step_ok = false;
		} else {
			__local_irq_disable();
//			printf("BEFORE\n");
//			dump_state();
//			printf("hello: %x\n", read_pc());
			step_ok = block->execute(this, reg_state());
//			printf("AFTER\n");
//			dump_state();
			__local_irq_enable();
		}
	} while(step_ok);

	return true;
}

bool CPU::run_region_jit()
{
	return false;
}

static GuestBasicBlock basic_block_cache[1024];

GuestBasicBlock* CPU::get_basic_block(uint32_t block_addr)
{
	GuestBasicBlock *cache_slot = &basic_block_cache[block_addr % 1024];
	if (cache_slot->block_address() == 0 || cache_slot->block_address() != block_addr) {
		if (cache_slot->block_address() != 0) {
			cache_slot->release_memory();
		}

		if (!compile_basic_block(block_addr, cache_slot)) {
			return NULL;
		}
	}

	/*GuestBasicBlock *cache_slot = &basic_block_cache[0];
	if (!compile_basic_block(block_addr, cache_slot))
		return NULL;*/

	return cache_slot;
}

bool CPU::compile_basic_block(uint32_t block_addr, GuestBasicBlock *block)
{
	TranslationContext ctx(&shmem->ir_buffer[0]);
	uint8_t decode_data[128];
	Decode *insn = (Decode *)&decode_data[0];

	uint32_t pc = block_addr;
	do {
		if (!decode_instruction(pc, insn)) {
			printf("cpu: unhandled decode fault @ %08x\n", pc);
			return false;
		}

		printf("jit: translating insn @ [%08x] %s\n", insn->pc, trace().disasm().disassemble(insn->pc, decode_data));

		ctx.add_instruction(jit::IRInstructionBuilder::create_streg(jit::IRInstructionBuilder::create_constant32(insn->pc), jit::IRInstructionBuilder::create_constant32(60)));
		ctx.add_instruction(jit::IRInstructionBuilder::create_verify());
		if (!jit().translate(insn, ctx)) {
			printf("jit: instruction translation failed\n");
			return false;
		}

		pc += insn->length;
	} while (!insn->end_of_block);

	ctx.add_instruction(jit::IRInstructionBuilder::create_ret());

	GuestBasicBlock::GuestBasicBlockFn fn = ctx.compile();

	if (fn) {
		block->fnptr(fn);
		block->block_address(block_addr);
		return true;
	} else {
		return false;
	}
}

bool CPU::verify_check()
{
	if (!shmem->options.verify) return true;

	Barrier *enter = (Barrier *)shmem->verify_shm_data->barrier_enter;
	Barrier *exit = (Barrier *)shmem->verify_shm_data->barrier_exit;

	if (shmem->options.verify_id == 1) {
		memcpy(shmem->verify_shm_data->data, reg_state(), reg_state_size());
	}

	enter->wait(shmem->options.verify_id);

	if (shmem->options.verify_id == 0) {
		if (memcmp(reg_state(), shmem->verify_shm_data->data, reg_state_size())) {
			shmem->verify_shm_data->fail = 1;
		} else {
			shmem->verify_shm_data->fail = 0;
		}

	}

	exit->wait(shmem->options.verify_id);

	if (shmem->verify_shm_data->fail) {
		printf("*** DIVERGENCE DETECTED ***\n");
		dump_state();
		return false;
	} else {
		//printf("OK\n");
		return true;
	}
}
