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
