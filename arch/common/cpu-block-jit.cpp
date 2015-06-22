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

//#define REG_STATE_PROTECTION
//#define DEBUG_TRANSLATION

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

		//printf("cpu: memory fault %d\n", rc);

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

		// This will perform a FETCH with side effects, so that we can impose the
		// correct permissions checking for the block we're about to execute.
		MMU::resolution_fault fault;
		if (!mmu().virt_to_phys(virt_pc, phys_pc, fault)) abort();

		// If there was a fault, then switch back to the safe-point.
		if (unlikely(fault)) {
			restore_safepoint(&cpu_safepoint, (int)fault);
			assert(false);
		}

		struct block_txln_cache_entry *cache_entry = get_block_txln_cache_entry(phys_pc);
		if (cache_entry->tag != phys_pc) {
			if (cache_entry->tag != 1 && cache_entry->fn) {
				free((void *)cache_entry->fn);
			}

			cache_entry->tag = phys_pc;
			cache_entry->fn = NULL;

			interpret_block();
		} else {
			va_t va_of_phys_page = (va_t)(0x100000000ULL | (uint64_t)(phys_pc & ~0xfffULL));
			mmu().set_page_executed(va_of_phys_page);

			if (cache_entry->fn == NULL) {
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

				// Update the cache entry function pointer
				cache_entry->fn = fn;

				// Disable writes in the MMU for detecting SMC
				mmu().disable_writes();
			}

			// Make sure we're in the correct privilege mode
			ensure_privilege_mode();

			_exec_txl = true;
			step_ok = (cache_entry->fn(&jit_state) == 0);
			_exec_txl = false;
		}
	} while(step_ok);

	return true;
}

void CPU::clear_block_cache()
{
	printf("jit: clearing block cache\n");
#ifdef DEBUG_TRANSLATION
	printf("jit: clearing block cache\n");
#endif

	for (struct block_txln_cache_entry *entry = block_txln_cache; entry < &block_txln_cache[block_txln_cache_size]; entry++) {
		if (entry->tag != 1 && entry->fn) {
			free((void *)entry->fn);
			entry->fn = NULL;
		}

		entry->tag = 1;
	}
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
	gpa_t page = (pc & ~0xfffULL);
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
		
		/*if (unlikely(cpu_data().verbose_enabled)) {
			ctx.add_instruction(IRInstruction::count(IROperand::pc(insn->pc), IROperand::const32(0)));
		}*/

		// Translate this instruction into the context.
		if (!jit().translate(insn, ctx)) {
			allocator.free(tb.ir_insn);
			return false;
		}

		pc += insn->length;
	} while (!insn->end_of_block && (pc & ~0xfffULL) == page);

	// Finish off with a RET.
	ctx.add_instruction(IRInstruction::ret());

	return true;
}
