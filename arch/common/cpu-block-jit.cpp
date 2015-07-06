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
#include <list>

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
		
		if (unlikely(cpu_data().isr)) {
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
			
			// Since we've just destroyed the stack, we should never get here.
			assert(false);
		}
		
		// Mark the physical page corresponding to the PC as executed
		mmu().set_page_executed(VA_OF_GPA(PAGE_ADDRESS_OF(phys_pc)));
		
		// Signal the hypervisor to make a profiling run
		if (unlikely(trace_interval > 5000000)) {
			analyse_blocks();
			trace_interval = 0;
		} else {
			trace_interval++;
		}
		
		struct region_txln_cache_entry *rgn_cache_entry = get_region_txln_cache_entry(phys_pc);
		if (rgn_cache_entry->tag == PAGE_ADDRESS_OF(phys_pc) && rgn_cache_entry->txln) {
			if (rgn_cache_entry->txln->native_fn_ptr(&jit_state) == 1)
				continue;
			
			if (virt_pc != read_pc())
				continue;
		} else if (unlikely(rgn_cache_entry->tag != PAGE_ADDRESS_OF(phys_pc))) {
			assert(rgn_cache_entry->tag == 1);
			rgn_cache_entry->tag = PAGE_ADDRESS_OF(phys_pc);
			
			if (unlikely(rgn_cache_entry->image == NULL)) {
				rgn_cache_entry->image = (shared::RegionImage *)shalloc(sizeof(shared::RegionImage));
				rgn_cache_entry->image->rwu = NULL;
			}
		}
		
		struct block_txln_cache_entry *blk_cache_entry = get_block_txln_cache_entry(phys_pc);
		if (unlikely(blk_cache_entry->tag != phys_pc)) {
			if (blk_cache_entry->tag != 1 && blk_cache_entry->txln) {
				release_block_translation(blk_cache_entry->txln);
			}

			blk_cache_entry->tag = phys_pc;
			blk_cache_entry->txln = NULL;
			blk_cache_entry->count = 1;

			//__local_irq_disable();
			step_ok = interpret_block();
			//__local_irq_enable();
		} else {
			if (rgn_cache_entry->image->rwu == NULL) blk_cache_entry->count++;

			if (unlikely(blk_cache_entry->txln == NULL)) {
				if (unlikely(blk_cache_entry->count < 10)) {
					step_ok = interpret_block();
					continue;
				}
				
				blk_cache_entry->txln = compile_block(phys_pc);				
				if (unlikely(!blk_cache_entry->txln)) {
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
			step_ok = ((blk_cache_entry->txln->native_fn_ptr)(&jit_state) == 0);
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

void CPU::analyse_blocks()
{
	for (uint32_t region_index = 0; region_index < region_txln_cache_size; region_index++) {
		if (region_txln_cache[region_index].tag == 1) continue;
		if (!region_txln_cache[region_index].image) continue;
		if (region_txln_cache[region_index].image->rwu) continue;
		
		shared::RegionWorkUnit *rwu = (shared::RegionWorkUnit *)shalloc(sizeof(shared::RegionWorkUnit));
		region_txln_cache[region_index].image->rwu = rwu;

		rwu->region_index = region_index;
		rwu->blocks = NULL;
		rwu->block_count = 0;
		rwu->valid = 1;
		
		for (uint32_t block_offset = 0; block_offset < 0x1000; block_offset+=2) {
			struct block_txln_cache_entry *blk_cache_entry = get_block_txln_cache_entry((region_index << 12) | block_offset);
			if (blk_cache_entry->tag == 1 || (blk_cache_entry->tag >> 12) != region_index || (blk_cache_entry->txln == NULL)) continue;
			if (blk_cache_entry->count < 20) continue;
			blk_cache_entry->count = 0;
			
			// add block to rwu
			rwu->block_count++;
			rwu->blocks = (shared::BlockWorkUnit *)shrealloc(rwu->blocks, sizeof(shared::BlockWorkUnit) * rwu->block_count);
			rwu->blocks[rwu->block_count - 1].offset = block_offset;
			
			uint32_t native_ir_size = blk_cache_entry->txln->ir_count * sizeof(shared::IRInstruction);
			shared::IRInstruction *ir_copy = (shared::IRInstruction *)shalloc(native_ir_size);
			memcpy((void *)ir_copy, blk_cache_entry->txln->ir, native_ir_size);
			
			rwu->blocks[rwu->block_count - 1].ir = ir_copy;
			rwu->blocks[rwu->block_count - 1].ir_count = blk_cache_entry->txln->ir_count;
		}
		
		if (rwu->block_count > 0) {
			asm volatile("out %0, $0xff" :: "a"(14), "D"(rwu));
		} else {
			shfree((void *)rwu);
		}
	}
}

void CPU::register_region(shared::RegionWorkUnit* rwu)
{
	if (rwu->fn_ptr == NULL) {
		//shfree(rwu);
		return;
	}
	
	struct region_txln_cache_entry *rgn_cache_entry = get_region_txln_cache_entry(rwu->region_index << 12);
	
	if (rwu->valid) {
		if (rgn_cache_entry->txln) {
			release_region_translation(rgn_cache_entry->txln);
		}
		
		rgn_cache_entry->txln = alloc_region_translation();
		rgn_cache_entry->txln->native_fn_ptr = (shared::region_txln_fn)rwu->fn_ptr;
		//printf("register %08x %p\n", rwu->region_index << 12, rgn_cache_entry->txln->native_fn_ptr);
	} else {
		shfree(rwu->fn_ptr);
	}
	
	rgn_cache_entry->image->rwu = NULL;
	
	// TODO: MEMORY LEAK
	//shfree(rwu);
	
	mmu().disable_writes();
}
