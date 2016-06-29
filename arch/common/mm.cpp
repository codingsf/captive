#include <mm.h>
#include <printf.h>
#include <string.h>
#include <malloc/page-allocator.h>

using namespace captive::arch;
using namespace captive::arch::malloc;

uint64_t Memory::__force_order;

void Memory::init()
{
}

extern "C" void __fast_zero_page(void *p);

uintptr_t Memory::alloc_pgt()
{
	uintptr_t va = (uintptr_t)page_alloc.alloc_page();
	uintptr_t pa = HVA_TO_HPA(va);
	
	//printf("mm: alloc page pa=%lx va=%lx\n", pa, va);
	
	__fast_zero_page((void *)va);
	//zero_page((void *)va);
	return pa;
}

void Memory::disable_caching(hva_t va)
{
	page_map_entry_t* pm;
	page_dir_ptr_entry_t* pdp;
	page_dir_entry_t* pd;
	page_table_entry_t* pt;

	get_va_table_entries(va, pm, pdp, pd, pt);
	
	pt->cache_disabled(true);
	pt->write_through(true);

	flush_page(va);
}
