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

//#define REG_STATE_PROTECTION
//#define DEBUG_TRANSLATION

bool CPU::run_block_jit_safepoint()
{
	bool step_ok = true;
	do {
		switch_to_kernel_mode();

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

		gva_t virt_pc = (gva_t)read_pc();
		gpa_t phys_pc;

		mmu().set_page_executed(virt_pc);

		MMU::access_info info;
		info.mode = kernel_mode() ? MMU::ACCESS_KERNEL : MMU::ACCESS_USER;
		info.type = MMU::ACCESS_FETCH;

		MMU::resolution_fault fault;

		// Grab the GPA for the GVA of the PC.
		if (!mmu().resolve_gpa(virt_pc, phys_pc, info, fault, false)) {
			printf("cpu: unable to resolve gva %08x\n", virt_pc);
			return false;
		}

		// If we faulted on the fetch, go via the interpreter to sort it out.
		if (fault != MMU::NONE) {
			printf("cpu: fault when fetching next block instruction @ %08x\n", virt_pc);
			abort();
			interpret_block();
			continue;
		}

		block_txln_cache_t::iterator txln = block_txln_cache.find(phys_pc);
		if (txln == block_txln_cache.end()) {
#ifdef REG_STATE_PROTECTION
			uint32_t tmp = Memory::get_va_flags(reg_state());
			Memory::set_va_flags(reg_state(), 0);
#endif

#ifdef DEBUG_TRANSLATION
			printf("jit: translating block phys-pc=%08x, virt-pc=%08x\n", phys_pc, virt_pc);
#endif

			shared::TranslationBlock tb;
			bzero(&tb, sizeof(tb));

			if (!translate_block(phys_pc, tb)) {
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

#ifdef REG_STATE_PROTECTION
			Memory::set_va_flags(reg_state(), tmp);
#endif

#ifdef DEBUG_TRANSLATION
			printf("jit: executing fresh block %p phys-pc=%08x, virt-pc=%08x\n", fn, phys_pc, virt_pc);
#endif
			block_txln_cache[phys_pc] = fn;

			ensure_privilege_mode();
			step_ok = (fn(&jit_state) == 0);
		} else {
			//printf("jit: executing cached block %x:%p\n", pc, txln->second);
			ensure_privilege_mode();
			step_ok = (txln->second(&jit_state) == 0);
		}
	} while(step_ok);

	return true;
}

void CPU::clear_block_cache()
{
#ifdef DEBUG_TRANSLATION
	printf("jit: clearing block cache\n");
#endif

	for (auto txln : block_txln_cache) {
		free((void *)txln.second);
	}

	block_txln_cache.clear();
}

bool CPU::translate_block(gpa_t pa, shared::TranslationBlock& tb)
{
	using namespace captive::shared;

	LocalMemory allocator;
	TranslationContext ctx(allocator, tb);
	uint8_t decode_data[128];
	Decode *insn = (Decode *)&decode_data[0];

	// We MUST begin in block zero.
	assert(ctx.current_block() == 0);

#ifdef DEBUG_TRANSLATION
	printf("jit: translating block %x\n", pa);
#endif

	tb.block_addr = pa;
	tb.heat = 0;
	tb.is_entry = 1;

	gpa_t pc = pa;
	do {
		// Attempt to decode the current instruction.
		if (!decode_instruction_phys(pc, insn)) {
			printf("jit: unhandled decode fault @ %08x\n", pc);
			assert(false);
		}

#ifdef DEBUG_TRANSLATION
		printf("jit: translating insn @ [%08x] %s\n", insn->pc, trace().disasm().disassemble(insn->pc, decode_data));
#endif

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
