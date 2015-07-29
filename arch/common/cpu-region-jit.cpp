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

#include <profile/image.h>
#include <profile/region.h>
#include <profile/block.h>

#include <set>
#include <map>
#include <list>

//#define REG_STATE_PROTECTION
//#define DEBUG_TRANSLATION

extern safepoint_t cpu_safepoint;

using namespace captive::arch;
using namespace captive::arch::jit;
using namespace captive::arch::profile;

bool CPU::run_region_jit()
{
	printf("cpu: starting region-jit cpu execution\n");
	
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

	return run_region_jit_safepoint();
}

bool CPU::run_region_jit_safepoint()
{
	uint32_t trace_interval = 0;
	bool step_ok = true;
	
	bool reset_trace = true;
	gpa_t last_phys_pc = 0;
	
	Region *rgn = NULL;
	uint32_t region_virt_base = 1;
	
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
		
		// If we've changed page...
		if (PAGE_ADDRESS_OF(virt_pc) != region_virt_base) {
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

			rgn = image->get_region(phys_pc);
			region_virt_base = PAGE_ADDRESS_OF(virt_pc);
		}		
		
		// Signal the hypervisor to make a profiling run
		if (unlikely(trace_interval > 3000000)) {
			reset_trace = true;
			
			analyse_blocks();
			trace_interval = 0;
		} else {
			trace_interval++;
		}
		
		Block *blk = rgn->get_block(PAGE_OFFSET_OF(virt_pc));
		
		if (unlikely(reset_trace) || PAGE_INDEX_OF(last_phys_pc) != PAGE_INDEX_OF(phys_pc)) {
			blk->entry = true;
			reset_trace = false;
		} else {
			if (last_phys_pc > phys_pc) {
				rgn->get_block(last_phys_pc)->loop_header = true;
			}
		}
				
		last_phys_pc = phys_pc;
		
		if (rgn->txln) {
			((void **)jit_state.region_txln_cache)[virt_pc >> 12] = (void *)rgn->txln;
			rgn->txln(&jit_state);
			if (virt_pc != read_pc()) continue;
		}
		
		if (rgn->rwu == NULL) blk->exec_count++;
		
		if (blk->txln) {
			step_ok = blk->txln(&jit_state) == 0;
			continue;
		}

		if (blk->exec_count > 10) {
			blk->txln = compile_block(blk, PAGE_ADDRESS_OF(phys_pc) | PAGE_OFFSET_OF(virt_pc));
			mmu().disable_writes();
			
			step_ok = blk->txln(&jit_state) == 0;
		} else {
			interpret_block();
		}
	} while(step_ok);

	return true;
}

void CPU::analyse_blocks()
{
	for (int ri = 0; ri < 0x100000; ri++) {
		Region *rgn = image->regions[ri];
		if (!rgn) continue;
		if (rgn->rwu) continue;
		
		for (int bi = 0; bi < 0x1000; bi++) {
			Block *blk = rgn->blocks[bi];
			if (!blk) continue;
			
			if (blk->exec_count >= 100) {
				compile_region(rgn, ri);
				break;
			}
		}
	}
}

void CPU::compile_region(Region *rgn, uint32_t region_index)
{
	rgn->rwu = (shared::RegionWorkUnit *) shalloc(sizeof(shared::RegionWorkUnit));
	rgn->rwu->region_index = region_index;
	rgn->rwu->valid = 1;
	rgn->rwu->block_count = 0;
	rgn->rwu->blocks = NULL;
	
	//printf("compiling region %p %08x\n", rgn, region_index << 12);
	
	for (int bi = 0; bi < 0x1000; bi++) {
		Block *blk = rgn->blocks[bi];
		if (!blk) continue;
		
		blk->exec_count = 0;
		
		if (!blk->ir) continue;
		
		rgn->rwu->block_count++;
		rgn->rwu->blocks = (shared::BlockWorkUnit *) shrealloc(rgn->rwu->blocks, sizeof(shared::BlockWorkUnit) * rgn->rwu->block_count);
		
		shared::BlockWorkUnit *bwu = &rgn->rwu->blocks[rgn->rwu->block_count - 1];
		bwu->offset = bi;
		bwu->interrupt_check = blk->loop_header || blk->entry;
		bwu->entry_block = blk->entry;
		
		bwu->ir_count = blk->ir_count;
		bwu->ir = (const shared::IRInstruction *)shalloc(sizeof(shared::IRInstruction) * bwu->ir_count);
		memcpy((void *)bwu->ir, (const void *)blk->ir, sizeof(shared::IRInstruction) * bwu->ir_count);
	}
	
	asm volatile("out %0, $0xff" :: "a"(14), "D"(rgn->rwu));
}

void CPU::register_region(shared::RegionWorkUnit* rwu)
{
	Region *rgn = image->get_region_from_index(rwu->region_index);
	assert(rgn->rwu == rwu);
	rgn->rwu = NULL;

	//printf("registering region %p %08x\n", rgn, rwu->region_index << 12);
	if (rwu->valid) {
		if (rgn->txln)
			shfree((void *)rgn->txln);
		
		rgn->txln = (shared::region_txln_fn)rwu->fn_ptr;
	} else {
		shfree(rwu->fn_ptr);
	}
	
	for (int i = 0; i < rwu->block_count; i++) {
		shfree((void *)rwu->blocks[i].ir);
	}
	
	shfree(rwu->blocks);
	shfree(rwu);
	
	mmu().disable_writes();
}
