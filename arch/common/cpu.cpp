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

using namespace captive::arch;
using namespace captive::arch::jit;

safepoint_t cpu_safepoint;

CPU *CPU::current_cpu;

CPU::CPU(Environment& env, PerCPUData *per_cpu_data) : _env(env), _per_cpu_data(per_cpu_data)
{
	// Zero out the local state.
	bzero(&local_state, sizeof(local_state));

	// Initialise the decode cache
	memset(decode_cache, 0xff, sizeof(decode_cache));

	// Initialise the block cache
	memset(block_cache, 0xff, sizeof(block_cache));
}

CPU::~CPU()
{

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

	// Create a safepoint for returning from a memory access fault
	int rc = record_safepoint(&cpu_safepoint);
	if (rc > 0) {
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

		uint32_t pc = read_pc();

		jit::GuestBasicBlock *block = get_block(pc);
		if (block->block_address() != pc) {
			if (block->valid()) block->release_memory();

			printf("jit: compiling at %x\n", pc);
			if (!compile_basic_block(pc, block)) {
				printf("jit: compilation of block %08x failed\n", pc);
				abort();
			}
		}

		mmu().set_page_executed(pc);

		// Execute the block, with interrupts disabled.
		__local_irq_disable();
		step_ok = block->execute(this, reg_state());
		__local_irq_enable();
	} while(step_ok);

	return true;
}

bool CPU::run_region_jit()
{
	return false;
}

bool CPU::interpret_block()
{
	bool step_ok = false;
	uint32_t prev_pc = 0x1;

	// Now, execute one basic-block of instructions.
	Decode *insn;
	do {
		// Reset CPU exception state
		local_state.last_exception_action = 0;

		// Get the address of the next instruction to execute
		uint32_t pc = read_pc();

		// Obtain a decode object for this PC, and perform the decode.
		insn = get_decode(pc);
		if (insn->pc != pc) {
			if (unlikely(!decode_instruction(pc, insn))) {
				printf("cpu: unhandled decode fault @ %08x\n", pc);
				return false;
			}
		}

		// Perhaps trace this instruction
		if (unlikely(trace().enabled())) {
			trace().start_record(get_insns_executed(), pc, insn);
		}

		if (unlikely(prev_pc >> 12 != pc >> 12)) {
			// Mark the page as has_been_executed
			mmu().set_page_executed(pc);
			prev_pc = pc;
		}

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

void CPU::invalidate_executed_page(va_t page_base_addr)
{
	if (page_base_addr > (va_t)0x100000000) return;

	// Evict entries from the decode cache and block cache
	uint32_t page_base = (uint32_t)(uint64_t)page_base_addr;
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
	}
}

GuestBasicBlock* CPU::get_basic_block(uint32_t block_addr)
{
	GuestBasicBlock *cache_slot = get_block(block_addr);

	if (cache_slot->block_address() == 0 || cache_slot->block_address() != block_addr) {
		if (cache_slot->block_address() != 0) {
			printf("jit: evicting %x\n", cache_slot->block_address());
			cache_slot->release_memory();
		}

		if (!compile_basic_block(block_addr, cache_slot)) {
			return NULL;
		}
	}

	/*GuestBasicBlock *cache_slot = &basic_block_cache[0].bb[0];
	if (cache_slot->block_address() != block_addr) {
		if (!compile_basic_block(block_addr, cache_slot)) {
			return NULL;
		}
	}*/

	return cache_slot;
}

bool CPU::compile_basic_block(uint32_t block_addr, GuestBasicBlock *block)
{
	TranslationContext ctx(cpu_data().guest_data->ir_buffer, cpu_data().guest_data->ir_buffer_size, 0, (uint64_t)cpu_data().guest_data->code_buffer);
	uint8_t decode_data[128];
	Decode *insn = (Decode *)&decode_data[0];

	//printf("jit: translating block %08x\n", block_addr);
	uint32_t pc = block_addr;
	do {
		// Attempt to decode the current instruction.
		if (!decode_instruction(pc, insn)) {
			printf("cpu: unhandled decode fault @ %08x\n", pc);
			return false;
		}

		//printf("jit: translating insn @ [%08x] %s\n", insn->pc, trace().disasm().disassemble(insn->pc, decode_data));

		// If verification is enabled, emit a PC update and a VERIFY
		// opcode.
		if (unlikely(cpu_data().verify_enabled)) {
			ctx.add_instruction(jit::IRInstructionBuilder::create_streg(jit::IRInstructionBuilder::create_constant32(insn->pc), jit::IRInstructionBuilder::create_constant32(60)));
			ctx.add_instruction(jit::IRInstructionBuilder::create_verify());
		}

		// Translate this instruction into the context.
		if (!jit().translate(insn, ctx)) {
			printf("jit: instruction translation failed\n");
			return false;
		}

		pc += insn->length;
	} while (!insn->end_of_block);

	// Finish off with a RET.
	ctx.add_instruction(jit::IRInstructionBuilder::create_ret());

	// TODO: Thread jumps.

	// Compile the translation context, and retrieve a function pointer.
	GuestBasicBlock::GuestBasicBlockFn fn = ctx.compile();

	// If compilation succeeded, update the GBB structure.
	if (fn) {
		block->update(block_addr, fn, 0);
		return true;
	} else {
		block->invalidate();
		return false;
	}
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
