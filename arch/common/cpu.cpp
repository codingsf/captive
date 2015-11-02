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

CPU *CPU::current_cpu;

CPU::CPU(Environment& env, PerCPUData *per_cpu_data)
	: _env(env),
	_per_cpu_data(per_cpu_data),
	_exec_txl(false),
	region_txln_cache(new region_txln_cache_t()),
	block_txln_cache(new block_txln_cache_t())
{
	// Zero out the local state.
	bzero(&local_state, sizeof(local_state));
	bzero(&tagged_reg_offsets, sizeof(tagged_reg_offsets));

	// Initialise the decode cache
	memset(decode_cache, 0xff, sizeof(decode_cache));

	// Initialise the profiling image
	image = new profile::Image();

	jit_state.cpu = this;
	
	jit_state.block_txln_cache = block_txln_cache->ptr();
	
	if (_per_cpu_data->execution_mode == 2) { // If we're a region jit
		jit_state.region_txln_cache = region_txln_cache->ptr();
	} else {
		jit_state.region_txln_cache = NULL;
	}
	
	jit_state.insn_counter = &(per_cpu_data->insns_executed);
	jit_state.exit_chain = 0;
	
	// Populate the FS register with the address of the JIT state structure.
	__wrmsr(0xc0000100, (uint64_t)&jit_state);
	
	// Populate the GS register with the address of the emulated user page table.
	__wrmsr(0xc0000101, (uint64_t)0x400000000ULL);	// GS Base
	__wrmsr(0xc0000102, (uint64_t)0x400000000ULL);	// Kernel GS Base

	invalidate_virtual_mappings();
}

CPU::~CPU()
{
	if (region_txln_cache)
		delete region_txln_cache;

	if (block_txln_cache)
		delete block_txln_cache;
}

bool CPU::handle_pending_action(uint32_t action)
{
	switch (action) {
	case 2:
	{
		//struct mallinfo mi = dlmallinfo();
		//printf("*** malloc info ***\n");
		//printf("used: %d\nfree: %d\n", mi.uordblks, mi.fordblks);

		dump_state();
		return true;
	}

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
		return false;
	case 1:
		return run_block_jit();
	case 2:
		return run_region_jit();
	case 3:
		return run_test();
	default:
		return false;
	}
}

bool CPU::run_test()
{
	volatile uint32_t *v1 = (volatile uint32_t *)0x1000;
	volatile uint32_t *v2 = (volatile uint32_t *)0x100001000;

	*v1; *v2;

	printf("dirty: map0=%d map1=%d val0=%d val1=%d\n", mmu().is_page_dirty((va_t)v1), mmu().is_page_dirty((va_t)v2), *v1, *v2);
	*v1 = 1;
	printf("dirty: map0=%d map1=%d val0=%d val1=%d\n", mmu().is_page_dirty((va_t)v1), mmu().is_page_dirty((va_t)v2), *v1, *v2);


	return true;
}

void CPU::invalidate_translations()
{
	image->invalidate();
	invalidate_virtual_mappings();
}

void CPU::invalidate_translation(pa_t phys_addr, va_t virt_addr)
{
	if (virt_addr >= (va_t)0x100000000) return;
	
	Region *rgn = image->get_region((uint32_t)(uint64_t)phys_addr);

	if (rgn) {
		rgn->invalidate();
	}
	
	invalidate_virtual_mappings();
}

void CPU::invalidate_virtual_mappings()
{
	if (block_txln_cache) {
		block_txln_cache->invalidate_dirty();
	}

	if (region_txln_cache) {
		region_txln_cache->invalidate_dirty();
	}
}

void CPU::invalidate_virtual_mapping(gva_t va)
{
	if (block_txln_cache) {
		block_txln_cache->invalidate_entry(va >> 2);
	}

	if(region_txln_cache) {
		region_txln_cache->invalidate_entry(va >> 12);
	}
}

void CPU::handle_irq_raised(uint8_t irq_line)
{
	//printf("*** raised %d %d\n", irq_line, cpu_data().isr);
	jit_state.exit_chain = 1; //cpu_data().isr;
}

void CPU::handle_irq_rescinded(uint8_t irq_line)
{
	//printf("*** rescinded %d\n", irq_line);
	//jit_state.exit_chain = cpu_data().isr;
}

