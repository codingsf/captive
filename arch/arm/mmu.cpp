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
#define L1_TEMP_BASE	(va_t)0x220005000

bool ArmMMU::resolve_gpa(gva_t va, gpa_t& pa, resolution_fault& fault)
{
	arm_resolution_fault arm_fault = NONE;
	fault = arch::MMU::NONE;
	pa = 0xf0f0f0f0;

	if (!_enabled) {
		pa = va;
		return true;
	}

	//printf("mmu: resolve va=%08x, ttbr0=%x\n", va, _coco.TTBR0());

	uint16_t l1_idx = va >> 20;
	l1_descriptor *l1 = &((l1_descriptor *)temp_map(TTBR_TEMP_BASE, _coco.TTBR0(), 4))[l1_idx];

	//printf("l1: va=%x type=%d, base addr=%x, ap=%d, dom=%d\n", va, l1->type(), l1->base_addr(), l1->ap(), l1->domain());

	switch (l1->type()) {
	case l1_descriptor::TT_ENTRY_COARSE:
		if (!resolve_coarse_page(va, pa, arm_fault, l1))
			return false;
		break;
	case l1_descriptor::TT_ENTRY_FINE:
		if (!resolve_fine_page(va, pa, arm_fault, l1))
			return false;
		break;
	case l1_descriptor::TT_ENTRY_SECTION:
		if (!resolve_section(va, pa, arm_fault, l1))
			return false;
		break;
	case l1_descriptor::TT_ENTRY_FAULT:
		arm_fault = SECTION_FAULT;
		break;
	}

	if (arm_fault == NONE) {
		//printf("l1: resolved va=%x pa=%x @ %d\n", va, pa, _cpu.get_insns_executed());
		return true;
	}

	fault = FAULT;

	uint32_t fsr = (uint32_t)l1->domain() << 4;
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

bool ArmMMU::resolve_coarse_page(gva_t va, gpa_t& pa, arm_resolution_fault& fault, l1_descriptor *l1)
{
	uint16_t l2_idx = ((uint32_t)va >> 12) & 0xff;

	uint8_t *x = (uint8_t *)temp_map(L1_TEMP_BASE, l1->base_addr(), 1);
	l2_descriptor *l2 = &((l2_descriptor *)(x + (l1->base_addr() & 0xfff)))[l2_idx];

	//printf("resolving coarse descriptor: l1-base=%x l2-idx=%d va=%x, type=%d, base=%x, data=%x\n", l1->base_addr(), l2_idx, va, l2->type(), l2->base_addr(), l2->data);

	uint32_t dacr = _coco.DACR();
	dacr >>= l1->domain() * 2;
	dacr &= 0x3;

	switch (dacr) {
	case 0:
		printf("mmu: coarse: domain fail\n");
		fault = COARSE_DOMAIN;
		return true;

	case 1:
		/*printf("mmu: coarse: permission fail\n");
		fault = COARSE_PERMISSION;
		return true;*/
		break;

	case 3:
		break;
	}

	switch(l2->type()) {
	case l2_descriptor::TE_FAULT:
		fault = COARSE_FAULT;
		return true;
	case l2_descriptor::TE_LARGE:
		pa = (gpa_t)(l2->base_addr() | (((uint32_t)va) & 0xffff));
		return true;
	case l2_descriptor::TE_SMALL:
		pa = (gpa_t)(l2->base_addr() | (((uint32_t)va) & 0xfff));
		return true;
	case l2_descriptor::TE_TINY:
		printf("mmu: unsupported tiny page\n");
		return false;
	}

	return false;
}

bool ArmMMU::resolve_fine_page(gva_t va, gpa_t& pa, arm_resolution_fault& fault, l1_descriptor *l1)
{
	printf("mmu: unsupported fine page table\n");
	return false;
}

bool ArmMMU::resolve_section(gva_t va, gpa_t& pa, arm_resolution_fault& fault, l1_descriptor *l1)
{
	pa = (gpa_t)(l1->base_addr() | (((uint32_t)va) & 0xfffff));
	return true;
}
