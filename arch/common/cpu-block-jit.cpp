#include <cpu.h>
#include <interp.h>
#include <decode.h>
#include <disasm.h>
#include <jit.h>
#include <safepoint.h>
#include <local-memory.h>
#include <jit/translation-context.h>
#include <jit/block-compiler.h>
#include <shared-jit.h>

extern safepoint_t cpu_safepoint;

using namespace captive::arch;
using namespace captive::arch::jit;

bool CPU::run_block_jit()
{
	printf("cpu: starting block-jit cpu execution\n");

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

	return run_block_jit_safepoint();
}

bool CPU::run_block_jit_safepoint()
{
	bool step_ok = true;
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

		block_txln_cache_t::iterator txln = block_txln_cache.find(pc);
		if (txln == block_txln_cache.end()) {
			shared::TranslationBlock tb;
			if (!translate_block(pc, tb)) {
				printf("jit: block translation failed\n");
				return false;
			}

			BlockCompiler compiler(tb);
			block_txln_fn fn;
			if (!compiler.compile(fn)) {
				printf("jit: block compilation failed\n");
				return false;
			}

			// Release TB memory
			free(tb.ir_insn);

			printf("jit: executing fresh block %p\n", fn);
			block_txln_cache[pc] = fn;
			step_ok = (fn(&jit_state) == 0);
		} else {
			printf("jit: executing cached block %x:%p\n", pc, txln->second);
			step_ok = (txln->second(&jit_state) == 0);
		}
	} while(step_ok);

	return true;
}

bool CPU::translate_block(uint32_t va, shared::TranslationBlock& tb)
{
	using namespace captive::shared;

	LocalMemory allocator;
	TranslationContext ctx(allocator, tb);
	uint8_t decode_data[128];
	Decode *insn = (Decode *)&decode_data[0];

	// We MUST begin in block zero.
	assert(ctx.current_block() == 0);

	printf("jit: translating block %x\n", va);

	tb.block_addr = va;
	tb.heat = 0;
	tb.is_entry = 1;

	uint32_t pc = va;
	do {
		// Attempt to decode the current instruction.
		if (!decode_instruction_virt(pc, insn)) {
			printf("jit: unhandled decode fault @ %08x\n", pc);
			assert(false);
		}

		printf("jit: translating insn @ [%08x] %s\n", insn->pc, trace().disasm().disassemble(insn->pc, decode_data));

		if (unlikely(cpu_data().verify_enabled)) {
			ctx.add_instruction(IRInstruction::verify(IROperand::pc(insn->pc)));
		}

		// Translate this instruction into the context.
		if (!jit().translate(insn, ctx)) {
			allocator.free(tb.ir_insn);
			return false;
		}

		pc += insn->length;
	} while (!insn->end_of_block);

	assert(insn->end_of_block);
	JumpInfo jump_info = get_instruction_jump_info(insn);

	tb.destination_target = jump_info.target;
	if (insn->is_predicated) {
		if (jump_info.type == JumpInfo::DIRECT) {
			tb.destination_type = TranslationBlock::PREDICATED_DIRECT;
		} else {
			tb.destination_type = TranslationBlock::PREDICATED_INDIRECT;
		}

		tb.fallthrough_target = pc;
	} else {
		if (jump_info.type == JumpInfo::DIRECT) {
			tb.destination_type = TranslationBlock::NON_PREDICATED_DIRECT;
		} else {
			tb.destination_type = TranslationBlock::NON_PREDICATED_INDIRECT;
		}
	}

	// Finish off with a RET.
	ctx.add_instruction(IRInstruction::ret());

	return true;
}
