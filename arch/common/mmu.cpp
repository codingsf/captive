#include <mmu.h>

#include "include/mmu.h"

using namespace captive::arch;

MMU::MMU(Environment& env) : _env(env), pml4((pm_t)0x200001000)
{

}

MMU::~MMU()
{

}

bool MMU::clear_vma()
{
	// Clear PRESENT bit of mappings for 4GB area
	return true;
}

bool MMU::install_phys_vma()
{
	// Re-assert 1-1 mapping for physical memory VMAs
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
