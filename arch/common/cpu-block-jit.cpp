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

#include <set>
#include <map>

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
	uint32_t trace_interval = 0;
	bool step_ok = true;
	
	do {
		// Check the ISR to determine if there is an interrupt pending,
		// and if there is, instruct the interpreter to handle it.
		
		if (unlikely(cpu_data().isr && interrupts_enabled())) {
			if (interpreter().handle_irq(cpu_data().isr)) {
				cpu_data().interrupts_taken++;
			}
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
		if (unlikely(!mmu().virt_to_phys(virt_pc, phys_pc, fault))) abort();

		// If there was a fault, then switch back to the safe-point.
		if (unlikely(fault)) {
			restore_safepoint(&cpu_safepoint, (int)fault);
			assert(false);
		}
		
		// Mark the physical page corresponding to the PC as executed
		mmu().set_page_executed(VA_OF_GPA(PAGE_ADDRESS_OF(phys_pc)));
		
		// Signal the hypervisor to make a profiling run
		if (unlikely(trace_interval > 300000)) {
			_analysing = true;
			trace_interval = 0;

			asm volatile("out %0, $0xff" :: "a"(14), "D"(block_txln_cache));
		} else if (!_analysing) {
			trace_interval++;
		}
		
		struct block_txln_cache_entry *cache_entry = get_block_txln_cache_entry(phys_pc);
		if (unlikely(cache_entry->tag != phys_pc)) {
			if (cache_entry->tag != 1 && cache_entry->txln) {
				release_block_translation(cache_entry->txln);
			}

			cache_entry->tag = phys_pc;
			cache_entry->txln = NULL;
			cache_entry->count = 1;

			//__local_irq_disable();
			step_ok = interpret_block();
			//__local_irq_enable();
		} else {
			cache_entry->count++;

			if (unlikely(cache_entry->txln == NULL)) {
				if (unlikely(cache_entry->count < 10)) {
					step_ok = interpret_block();
					continue;
				}
				
				cache_entry->txln = compile_block(phys_pc);				
				if (unlikely(!cache_entry->txln)) {
					printf("jit: block compilation failed\n");
					return false;
				}
				
				// Disable writes in the MMU for detecting SMC
				mmu().disable_writes();
			}
			
			// Make sure we're in the correct privilege mode
			ensure_privilege_mode();

			//__local_irq_disable();
			
			_exec_txl = true;
			step_ok = ((cache_entry->txln->native_fn_ptr)(&jit_state) == 0);
			_exec_txl = false;
			
			//__local_irq_enable();
		}
	} while(step_ok);

	return true;
}

captive::shared::BlockTranslation *CPU::compile_block(gpa_t pa)
{
	TranslationContext ctx;

	if (!translate_block(ctx, pa)) {
		printf("jit: block translation failed\n");
		return NULL;
	}

	BlockCompiler compiler((void *)block_txln_cache, ctx, pa);
	captive::shared::block_txln_fn fn;
	if (!compiler.compile(fn)) {
		printf("jit: block compilation failed\n");
		return NULL;
	}
	
	shared::BlockTranslation *txln = alloc_block_translation();
	assert(txln);
	
	txln->native_fn_ptr = fn;
	txln->ir_count = ctx.count();
	txln->ir = ctx.get_ir_buffer();
	
	return txln;
}

bool CPU::translate_block(TranslationContext& ctx, gpa_t pa)
{
	using namespace captive::shared;
	
	// We MUST begin in block zero.
	assert(ctx.current_block() == 0);

#ifdef DEBUG_TRANSLATION
	printf("jit: translating block %x\n", pa);
#endif

	Decode *insn = get_decode(0);
			
	gpa_t pc = pa;
	gpa_t page = PAGE_ADDRESS_OF(pc);
	do {
		// Attempt to decode the current instruction.
		if (!decode_instruction_phys(pc, insn)) {
			printf("jit: unhandled decode fault @ %08x\n", pc);
			return false;
		}

#ifdef DEBUG_TRANSLATION
		printf("jit: translating insn @ [%08x] %s\n", insn->pc, trace().disasm().disassemble(insn->pc, decode_data));
#endif

		if (unlikely(cpu_data().verify_enabled)) {
			ctx.add_instruction(IRInstruction::verify(IROperand::pc(insn->pc)));
		}
		
		if (unlikely(cpu_data().verbose_enabled)) {
			ctx.add_instruction(IRInstruction::count(IROperand::pc(insn->pc), IROperand::const32(0)));
		}

		// Translate this instruction into the context.
		if (!jit().translate(insn, ctx)) {
			return false;
		}

		pc += insn->length;
	} while (!insn->end_of_block && PAGE_ADDRESS_OF(pc) == page);

	// Finish off with a RET.
	ctx.add_instruction(IRInstruction::ret());

	return true;
}

static std::set<uint32_t> compilation_set;

void CPU::analyse_blocks()
{
	typedef const struct block_txln_cache_entry *const_cache_entry_ptr;
	typedef std::list<const_cache_entry_ptr> block_list_t;
	typedef std::map<uint32_t, block_list_t> region_map_t;
	
	region_map_t regions;

	// Build a region map
	/*for (uint32_t cache_idx = 0; cache_idx < block_txln_cache_size; cache_idx++) {
		const_cache_entry_ptr cache_entry = &block_txln_cache[cache_idx];
		if (cache_entry->tag == 1) continue;
		
		block_list_t& blocks = regions[PAGE_ADDRESS_OF(cache_entry->tag)];
		blocks.push_back(cache_entry);
	}*/
	
	/*for (auto region : regions) {
		if (compilation_set.count(region.first)) continue;
		
		if (region.second.size() > 10) {
			printf("compiling region %x\n", region.first);
			compilation_set.insert(region.first);
		}
	}*/
}
