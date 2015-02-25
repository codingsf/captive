#include <arm-mmu.h>
#include <arm-cpu.h>
#include <arm-env.h>
#include <mm.h>
#include <printf.h>

using namespace captive::arch;
using namespace captive::arch::arm;

static const char *arm_faults[] = {
	"none",
	"other",

	"section-fault",
	"section-domain",
	"section-permission",

	"coarse-fault",
	"coarse-domain",
	"coarse-permission",
};

ArmMMU::ArmMMU(ArmCPU& cpu) : MMU(cpu), _coco((devices::CoCo&)*cpu.env().lookup_core_device(15))
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

bool ArmMMU::resolve_gpa(gva_t va, gpa_t& pa, const access_info& info, resolution_fault& fault)
{
	arm_resolution_fault arm_fault = NONE;
	fault = arch::MMU::NONE;
	pa = 0xf0f0f0f0;

	if (!_enabled) {
		pa = va;
		return true;
	}

	//printf("mmu: resolve va=%08x, type=%d, ttbr0=%x\n", va, type, _coco.TTBR0());

	uint16_t l1_idx = va >> 20;
	l1_descriptor *l1 = &((l1_descriptor *)temp_map(TTBR_TEMP_BASE, _coco.TTBR0(), 4))[l1_idx];

	//printf("l1: va=%x type=%d, base addr=%x, ap=%d, dom=%d\n", va, l1->type(), l1->base_addr(), l1->ap(), l1->domain());

	switch (l1->type()) {
	case l1_descriptor::TT_ENTRY_COARSE:
		if (!resolve_coarse_page(va, pa, info, arm_fault, l1))
			return false;
		break;
	case l1_descriptor::TT_ENTRY_FINE:
		if (!resolve_fine_page(va, pa, info, arm_fault, l1))
			return false;
		break;
	case l1_descriptor::TT_ENTRY_SECTION:
		if (!resolve_section(va, pa, info, arm_fault, l1))
			return false;
		break;
	case l1_descriptor::TT_ENTRY_FAULT:
		arm_fault = SECTION_FAULT;
		break;
	}

	if (arm_fault == NONE) {
		return true;
	}

	switch (info.type) {
	case MMU::ACCESS_READ:
		fault = MMU::READ_FAULT;
		break;
	case MMU::ACCESS_WRITE:
		fault = MMU::WRITE_FAULT;
		break;
	case MMU::ACCESS_FETCH:
		fault = MMU::FETCH_FAULT;
		break;
	}

	uint32_t fsr = (uint32_t)l1->domain() << 4;
	switch (arm_fault) {
	case ArmMMU::SECTION_FAULT:
		fsr |= 0x5;
		break;

	case ArmMMU::COARSE_FAULT:
		fsr |= 0x7;
		break;

	case ArmMMU::SECTION_DOMAIN:
		fsr |= 0x9;
		break;

	case ArmMMU::COARSE_DOMAIN:
		fsr |= 0xb;
		break;

	case ArmMMU::SECTION_PERMISSION:
		fsr |= 0xd;
		break;

	case ArmMMU::COARSE_PERMISSION:
		fsr |= 0xf;
		break;

	default:
		printf("mmu: unhandled resolution fault = %d\n", arm_fault);
		return false;
	}

	printf("mmu: fault: fsr=%x far=%x arm-fault=%s\n", fsr, va, arm_faults[arm_fault]);

	_coco.FSR(fsr);
	_coco.FAR(va);

	return true;
}

bool ArmMMU::check_access_perms(uint32_t ap, bool kernel_mode, bool is_write)
{
	switch(ap) {
	case 0:
		if(_coco.R()) return !is_write;
		else if(_coco.S()) return kernel_mode && !is_write;
		return false;

	case 1:
		return kernel_mode;

	case 2:
		return kernel_mode || (!is_write);

	case 3:
		return true;

	default:
		printf("mmu: unknown access permission type\n");
		return false;
	}
}

bool ArmMMU::resolve_coarse_page(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, l1_descriptor *l1)
{
	uint16_t l2_idx = ((uint32_t)va >> 12) & 0xff;

	uint8_t *x = (uint8_t *)temp_map(L1_TEMP_BASE, l1->base_addr(), 1);
	l2_descriptor *l2 = &((l2_descriptor *)(x + (l1->base_addr() & 0xfff)))[l2_idx];

	if (l2->type() == l2_descriptor::TE_FAULT) {
		fault = COARSE_FAULT;
		return true;
	}

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
		if (!check_access_perms(l2->ap0(), info.mode == ACCESS_KERNEL, info.type == ACCESS_WRITE)) {
			fault = COARSE_PERMISSION;
			return true;
		}

		break;

	case 3:
		break;
	}

	switch(l2->type()) {
	case l2_descriptor::TE_LARGE:
		pa = (gpa_t)(l2->base_addr() | (((uint32_t)va) & 0xffff));
		return true;
	case l2_descriptor::TE_SMALL:
		pa = (gpa_t)(l2->base_addr() | (((uint32_t)va) & 0xfff));
		return true;
	case l2_descriptor::TE_TINY:
		printf("mmu: unsupported tiny page\n");
		return false;
	default:
		printf("mmu: unsupported page type\n");
		return false;
	}

	return false;
}

bool ArmMMU::resolve_fine_page(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, l1_descriptor *l1)
{
	printf("mmu: unsupported fine page table\n");
	return false;
}

bool ArmMMU::resolve_section(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, l1_descriptor *l1)
{
	uint32_t dacr = _coco.DACR();
	dacr >>= l1->domain() * 2;
	dacr &= 0x3;

	switch(dacr) {
	case 0:
		fault = SECTION_DOMAIN;
		return true;

	case 1:
		if (!check_access_perms(l1->ap(), info.mode == ACCESS_KERNEL, info.type == ACCESS_WRITE)) {
			fault = SECTION_PERMISSION;
			return true;
		}

		break;

	case 3:
		break;
	}

	pa = (gpa_t)(l1->base_addr() | (((uint64_t)va) & 0xfffff));
	return true;
}
