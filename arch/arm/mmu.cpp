#include <arm-mmu.h>
#include <arm-cpu.h>
#include <arm-env.h>
#include <mm.h>
#include <printf.h>

using namespace captive::arch;
using namespace captive::arch::arm;

ArmMMU::ArmMMU(ArmCPU& cpu) : MMU(cpu.env()), _cpu(cpu), _coco((devices::CoCo&)*cpu.env().lookup_core_device(15))
{

}

ArmMMU::~ArmMMU()
{

}

bool ArmMMU::enable()
{
	if (_enabled) return true;

	clear_vma();

	_enabled = true;
	printf("mmu: enabled\n");
	return true;
}

bool ArmMMU::disable()
{
	if (!_enabled) return true;

	clear_vma();

	_enabled = false;
	printf("mmu: disabled\n");
	return true;
}

va_t ArmMMU::temp_map(va_t base, gpa_t gpa, int n)
{
	va_t vaddr = base;

	for (int i = 0; i < n; i++) {
		page_map_entry_t *pm;
		page_dir_ptr_entry_t *pdp;
		page_dir_entry_t *pd;
		page_table_entry_t *pt;

		Memory::get_va_table_entries(vaddr, pm, pdp, pd, pt);

		pt->base_address(0x100000000 | (uint64_t)gpa);
		pt->present(true);

		Memory::flush_page(vaddr);

		vaddr = (va_t)((uint64_t)vaddr + 0x1000);
		gpa = (gpa_t)((uint64_t)gpa + 0x1000);
	}

	return base;
}

#define TTBR_TEMP_BASE	(va_t)0x220000000
#define L1_TEMP_BASE	(va_t)0x220004000

bool ArmMMU::resolve_gpa(gva_t va, gpa_t& pa, resolution_fault& fault)
{
	arm_resolution_fault arm_fault = NONE;
	fault = arch::MMU::NONE;

	if (!_enabled) {
		pa = va;
		return true;
	}

	//printf("mmu: resolve va=%08x, ttbr0=%x\n", va, _coco.TTBR0());

	// Acquire the actual TTBR
	l0_entry *ttbr = (l0_entry *)temp_map(TTBR_TEMP_BASE, _coco.TTBR0(), 4);
	if (!ttbr) {
		return false;
	}

	// Calculate the ttbr index, and grab the entry
	uint16_t ttbr_idx = va >> 20;
	l0_entry *ttbr_entry = &ttbr[ttbr_idx];

	//printf("ttb entry: va=%x type=%d, base addr=%x, ap=%d, dom=%d\n", va, ttbr_entry->type(), ttbr_entry->base_addr(), ttbr_entry->ap(), ttbr_entry->domain());

	switch (ttbr_entry->type()) {
	case l0_entry::TT_ENTRY_COARSE:
		if (!resolve_coarse_page(va, pa, arm_fault, ttbr_entry))
			return false;
		break;
	case l0_entry::TT_ENTRY_FINE:
		if (!resolve_fine_page(va, pa, arm_fault, ttbr_entry))
			return false;
		break;
	case l0_entry::TT_ENTRY_SECTION:
		if (!resolve_section(va, pa, arm_fault, ttbr_entry))
			return false;
		break;
	case l0_entry::TT_ENTRY_FAULT:
		arm_fault = SECTION_FAULT;
		break;
	}

	if (arm_fault == NONE) {
		return true;
	}

	fault = FAULT;

	uint32_t fsr = (uint32_t)ttbr_entry->domain() << 4;
	switch (arm_fault) {
	case SECTION_FAULT:
		fsr |= 0x5;
		break;

	case COARSE_FAULT:
		fsr |= 0x7;
		break;

	case SECTION_PERMISSION:
		fsr |= 0xd;
		break;

	case COARSE_PERMISSION:
		fsr |= 0xf;
		break;

	default:
		printf("mmu: unhandled resolution fault = %d\n", arm_fault);
		return false;
	}

	printf("mmu: fault: fsr=%x far=%x %d\n", fsr, va, arm_fault);

	_coco.FSR(fsr);
	_coco.FAR(va);

	return true;
}

bool ArmMMU::resolve_coarse_page(gva_t va, gpa_t& pa, arm_resolution_fault& fault, l0_entry *entry)
{
	uint16_t l1_idx = ((uint32_t)va >> 12) & 0xff;
	l1_entry *l1 = (l1_entry *)temp_map(L1_TEMP_BASE, entry->base_addr(), 1);

	printf("resolving coarse descriptor for %x @ %x = %p, idx=%d, data=%x\n", va, entry->base_addr(), l1, l1_idx, l1[l1_idx].data);

	switch(l1[l1_idx].type()) {
	case l1_entry::TE_FAULT:
		fault = COARSE_FAULT;
		return true;
	case l1_entry::TE_LARGE:
		pa = (gpa_t)(l1[l1_idx].base_addr() | (((uint32_t)va) & 0xffff));
		return true;
	case l1_entry::TE_SMALL:
		pa = (gpa_t)(l1[l1_idx].base_addr() | (((uint32_t)va) & 0xfff));
		return true;
	case l1_entry::TE_TINY:
		printf("mmu: unsupported tiny page\n");
		return false;
	}
/*10000552
10000653*/
	return false;
}

bool ArmMMU::resolve_fine_page(gva_t va, gpa_t& pa, arm_resolution_fault& fault, l0_entry *entry)
{
	printf("ttb fine\n");
	return false;
}

bool ArmMMU::resolve_section(gva_t va, gpa_t& pa, arm_resolution_fault& fault, l0_entry *entry)
{
	//printf("ttb section va=%x offset=%x addr=%x\n", va, va_offset, entry.base_addr());

	pa = (gpa_t)(entry->base_addr() | (((uint32_t)va) & 0xfffff));
	return true;
}
