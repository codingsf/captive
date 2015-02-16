#include <mmu.h>
#include <printf.h>

using namespace captive::arch;

unsigned long MMU::__force_order;

MMU::MMU(Environment& env) : _env(env), pml4_phys(0x1000), pml4(NULL)
{
	pml4 = (pm_t)phys_to_virt(pml4_phys);
}

MMU::~MMU()
{

}

bool MMU::clear_vma()
{
	pm_t pm;
	pdp_t pdp;
	pd_t pd;
	pt_t pt;

	va_entries(0, pm, pdp, pd, pt);
	printf("mmu: clear vma: %p (%x) %p (%x) %p (%x) %p (%x)\n", pm, *pm, pdp, *pdp, pd, *pd, pt, *pt);

	// Clear PRESENT bit of mappings for 4GB area
	for (int pdp_idx = 0; pdp_idx < 4; pdp_idx++) {
		printf("mmu: pdp: %016x\n", pdp[pdp_idx]);
		pdp[pdp_idx] &= ~1ULL; // &= ~0xffdULL;			// Clear PRESENT bit
		printf("mmu: pdp: %016x\n", pdp[pdp_idx]);
	}

	// Flush TLB
	flush_tlb();
	flush_tlb_all();

	asm volatile("out %0, $0xff\n" :: "a"(3));

	uint64_t p = *(volatile uint64_t *)0x10000; //0x200000030; //0x10000; //ffffffff;
	printf("%x\n", p);

	return true;
}

bool MMU::install_phys_vma()
{
	pm_t pm;
	pdp_t pdp;
	pd_t pd;
	pt_t pt;

	va_entries(0, pm, pdp, pd, pt);
	printf("mmu: install phys mem: %p %p %p %p\n", pm, pdp, pd, pt);

	// Re-assert 1-1 mapping for physical memory VMAs
	for (int pdp_idx = 0; pdp_idx < 4; pdp_idx++) {
		printf("mmu: pdp: %x\n", pdp[pdp_idx]);
		pdp[pdp_idx] |= 1;				// Reassociate PRESENT bit
		printf("mmu: pdp: %x\n", pdp[pdp_idx]);
	}

	// Flush TLB
	flush_tlb();

	return true;
}

void MMU::map_page(page *pml4, uint64_t va, uint64_t pa)
{
	uint16_t pm, pdp, pd, pt;
	va_idx(va, pm, pdp, pd, pt);
}

void MMU::unmap_page(page *pml4, uint64_t va)
{
	uint16_t pm, pdp, pd, pt;
	va_idx(va, pm, pdp, pd, pt);
}
