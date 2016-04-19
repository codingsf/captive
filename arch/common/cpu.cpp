#include <cpu.h>
#include <barrier.h>
#include <mmu.h>
#include <decode.h>
#include <disasm.h>
#include <string.h>
#include <safepoint.h>
#include <priv.h>
#include <jit.h>
#include <jit/translation-context.h>
#include <profile/image.h>
#include <profile/region.h>
#include <profile/block.h>

using namespace captive::arch;
using namespace captive::arch::jit;
using namespace captive::arch::profile;

safepoint_t cpu_safepoint;

#ifndef NDEBUG
extern uint64_t page_faults;
extern uint64_t page_fault_reasons[8];
#endif

CPU *CPU::current_cpu;

CPU::CPU(Environment& env, PerCPUData *per_cpu_data)
	: _env(env),
	_per_cpu_data(per_cpu_data),
	block_txln_cache(new block_txln_cache_t()),
	_exec_txl(false)
{
	// Zero out the local state.
	bzero(&local_state, sizeof(local_state));
	bzero(&jit_state, sizeof(jit_state));
	bzero(&tagged_reg_offsets, sizeof(tagged_reg_offsets));

	local_state._kernel_mode = true;
	
	// Initialise the decode cache
	memset(decode_cache, 0xff, sizeof(decode_cache));

	// Initialise the profiling image
	image = new profile::Image();

	jit_state.cpu = this;
	jit_state.execution_mode = 0;
	
	jit_state.block_txln_cache = block_txln_cache->ptr();
	printf("block-txln-cache: %p\n", jit_state.block_txln_cache);
	
	jit_state.insn_counter = &(per_cpu_data->insns_executed);
	jit_state.exit_chain = 0;
	
	// Populate the FS register with the address of the JIT state structure.
	__wrmsr(0xc0000100, (uint64_t)&jit_state);
	
	// Populate the GS register with the address of the emulated user page table.
	__wrmsr(0xc0000101, (uint64_t)GPM_EMULATED_VIRT_START);	// GS Base
	__wrmsr(0xc0000102, (uint64_t)GPM_EMULATED_VIRT_START);	// Kernel GS Base

	invalidate_virtual_mappings();
}

CPU::~CPU()
{
	if (block_txln_cache)
		delete block_txln_cache;
}

bool CPU::handle_pending_action(uint32_t action)
{
	switch (action) {
	case 1:
#ifndef NDEBUG
		printf("page faults: %lu, [0]:%lu, [1]:%lu, [2]:%lu, [3]:%lu, [4]:%lu, [5]:%lu, [6]:%lu, [7]:%lu\n",
				page_faults,
				page_fault_reasons[0],
				page_fault_reasons[1],
				page_fault_reasons[2],
				page_fault_reasons[3],
				page_fault_reasons[4],
				page_fault_reasons[5],
				page_fault_reasons[6],
				page_fault_reasons[7]);
#endif
		return false;

	case 2:
		dump_state();
		return true;

	case 3:
		trace().enable();
		return true;
		
	case 4:
		printf("*** interrupt debug ***\n");
		printf("ISR=%u\n", cpu_data().isr);
		return true;
	}

	return false;
}

bool CPU::run()
{
	return run_block_jit();
}

bool CPU::run_test()
{
	volatile uint32_t *v1 = (volatile uint32_t *)0x1000;
	volatile uint32_t *v2 = (volatile uint32_t *)0x100001000;

	*v1; *v2;

	printf("dirty: map0=%d map1=%d val0=%d val1=%d\n", mmu().is_page_dirty((hva_t)v1), mmu().is_page_dirty((hva_t)v2), *v1, *v2);
	*v1 = 1;
	printf("dirty: map0=%d map1=%d val0=%d val1=%d\n", mmu().is_page_dirty((hva_t)v1), mmu().is_page_dirty((hva_t)v2), *v1, *v2);


	return true;
}

void CPU::invalidate_translations()
{
	image->invalidate();
	invalidate_virtual_mappings();
}

void CPU::invalidate_translation_phys(gpa_t phys_addr)
{
	Region *rgn = image->get_region((uint32_t)phys_addr & 0xfffff000);

	if (rgn) {
		rgn->invalidate();
	}
}

void CPU::invalidate_translation_virt(gva_t virt_addr)
{
	hpa_t phys_addr;
	if (Memory::quick_txl((virt_addr & 0xfffff000), phys_addr)) {
		invalidate_translation_phys((gpa_t)phys_addr);
	} else {
		image->invalidate();
	}
	
	invalidate_virtual_mappings();
}

void CPU::invalidate_virtual_mappings()
{
	block_txln_cache->invalidate_dirty();
}

void CPU::invalidate_virtual_mapping(gva_t va)
{
	block_txln_cache->invalidate_entry(va >> 2);
}

void CPU::handle_irq_raised()
{
	jit_state.exit_chain = 1;
}
