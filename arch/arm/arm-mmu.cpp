#include <arm-mmu.h>
#include <arm-cpu.h>
#include <arm-env.h>
#include <mm.h>
#include <printf.h>

//#define DEBUG_MMU

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

static const char *mem_access_types[] = { "read", "write", "fetch", "read-user", "write-user" };
static const char *mem_access_modes[] = { "user", "kernel" };

arm_mmu::arm_mmu(arm_cpu& cpu) : MMU(cpu), _coco((devices::CoCo&)*cpu.env().lookup_core_device(15)), _enabled(false)
{

}

arm_mmu::~arm_mmu()
{

}

bool arm_mmu::enable()
{
	if (_enabled) return true;

	invalidate_virtual_mappings();
	cpu().invalidate_translations();
	
	_enabled = true;
	return true;
}

bool arm_mmu::disable()
{
	if (!_enabled) return true;
	
	invalidate_virtual_mappings();
	cpu().invalidate_translations();

	_enabled = false;
	return true;
}

arm_mmu_v5::arm_mmu_v5(arm_cpu& cpu) : arm_mmu(cpu)
{

}

arm_mmu_v5::~arm_mmu_v5()
{

}

bool arm_mmu_v5::resolve_gpa(gva_t va, gpa_t& pa, const access_info& info, resolution_fault& fault, bool have_side_effects)
{
	arm_resolution_fault arm_fault = NONE;
	fault = arch::MMU::NONE;
	pa = 0xf0f0f0f0;
	
	if (!_enabled) {
		pa = va;
		return true;
	}
	
	//printf("mmu: va=%08x, TTBR0=%08x, TTBR1=%08x, TTBCR=%08x\n", va, *(armcpu().reg_offsets.TTBR0), *(armcpu().reg_offsets.TTBR1), *(armcpu().reg_offsets.TTBCR));
	//printf("mmu: resolve va=%08x, type=%d, ttbr0=%x\n", va, info.type, *(armcpu().reg_offsets.TTBR0));

	uint32_t ttbr = *armcpu().reg_offsets.TTBR0;
	
	l1_descriptor *ttb = (l1_descriptor *)resolve_guest_phys((gpa_t)(ttbr & ~0xfff));
	l1_descriptor *l1 = &ttb[va >> 20];

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
	case arm_mmu_v5::SECTION_FAULT:
		fsr |= 0x5;
		break;

	case arm_mmu_v5::COARSE_FAULT:
		fsr |= 0x7;
		break;

	case arm_mmu_v5::SECTION_DOMAIN:
		fsr |= 0x9;
		break;

	case arm_mmu_v5::COARSE_DOMAIN:
		fsr |= 0xb;
		break;

	case arm_mmu_v5::SECTION_PERMISSION:
		fsr |= 0xd;
		break;

	case arm_mmu_v5::COARSE_PERMISSION:
		fsr |= 0xf;
		break;

	default:
		printf("mmu: unhandled resolution fault = %d\n", arm_fault);
		return false;
	}

	//printf("mmu: fault: l1=%08x fsr=%x far=%x arm-fault=%s type=%s:%s\n", l1->data, fsr, va, arm_faults[arm_fault], mem_access_modes[info.mode], mem_access_types[info.type]);

	if (likely(have_side_effects)) {
		*(armcpu().reg_offsets.FSR) = fsr;
		*(armcpu().reg_offsets.FAR) = va;
	}

	return true;
}

bool arm_mmu_v5::check_access_perms(uint32_t ap, bool kernel_mode, bool is_write)
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

bool arm_mmu_v5::resolve_coarse_page(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, l1_descriptor *l1)
{
	l2_descriptor *coarse_table = (l2_descriptor *)resolve_guest_phys(l1->base_addr());
	l2_descriptor *l2 = &coarse_table[((uint32_t)va >> 12) & 0xff];

	if (l2->type() == l2_descriptor::TE_FAULT) {
		fault = COARSE_FAULT;
		return true;
	}

	//printf("resolving coarse descriptor: l1-base=%x l2-idx=%d va=%x, type=%d, base=%x, data=%x\n", l1->base_addr(), l2_idx, va, l2->type(), l2->base_addr(), l2->data);

	uint32_t dacr = *(armcpu().reg_offsets.DACR);
	dacr >>= l1->domain() * 2;
	dacr &= 0x3;

	switch (dacr) {
	case 0:
		fault = COARSE_DOMAIN;
		return true;

	case 1:
		if (!check_access_perms(l2->ap0(), info.mode == ACCESS_KERNEL, info.is_write())) {
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

bool arm_mmu_v5::resolve_fine_page(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, l1_descriptor *l1)
{
	printf("mmu: unsupported fine page table\n");
	return false;
}

bool arm_mmu_v5::resolve_section(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, l1_descriptor *l1)
{
	uint32_t dacr = *(armcpu().reg_offsets.DACR);
	dacr >>= l1->domain() * 2;
	dacr &= 0x3;

	switch(dacr) {
	case 0:
		fault = SECTION_DOMAIN;
		return true;

	case 1:
		if (!check_access_perms(l1->ap(), info.mode == ACCESS_KERNEL, info.is_write())) {
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

arm_mmu_v6::arm_mmu_v6(arm_cpu& cpu) : arm_mmu(cpu)
{

}

arm_mmu_v6::~arm_mmu_v6()
{

}

bool arm_mmu_v6::resolve_gpa(gva_t va, gpa_t& pa, const access_info& info, resolution_fault& fault, bool have_side_effects)
{
	arm_resolution_fault arm_fault = NONE;
	fault = arch::MMU::NONE;
	pa = 0xf0f0f0f0;
	
	if (!_enabled) {
		pa = va;
		return true;
	}

#ifdef DEBUG_MMU
	printf("mmu-v6: resolving %08x\n", va);
#endif
	
	uint32_t ttbr = *armcpu().reg_offsets.TTBR0;
	
	l1_descriptor *ttb = (l1_descriptor *)resolve_guest_phys((gpa_t)(ttbr & ~0xfff));
	l1_descriptor *l1 = &ttb[va >> 20];
	
#ifdef DEBUG_MMU
	printf("mmu-v6: resolved L1 @ %p data=%08x type=%d\n", l1, l1->data, l1->type());
#endif
	
	switch (l1->type()) {
	case l1_descriptor::SECTION:
		if (!resolve_section(va, pa, info, arm_fault, l1)) return false;
		break;
		
	case l1_descriptor::SUPERSECTION:
		fatal("nope\n");
		
	case l1_descriptor::COARSE_PAGE_TABLE:
		if (!resolve_coarse(va, pa, info, arm_fault, l1)) return false;
		break;
		
	case l1_descriptor::FAULT:
		fatal("nope\n");
	}

	if (arm_fault == NONE) {
#ifdef DEBUG_MMU
		printf("mmu-v6: resolved va=%08x => pa=%08x\n", va, pa);
#endif
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

	fatal("not yet supported\n");
	
	/*uint32_t fsr = (uint32_t)l1->domain() << 4;
	switch (arm_fault) {
	case arm_mmu_v6::SECTION_FAULT:
		fsr |= 0x5;
		break;

	case arm_mmu_v6::COARSE_FAULT:
		fsr |= 0x7;
		break;

	case arm_mmu_v6::SECTION_DOMAIN:
		fsr |= 0x9;
		break;

	case arm_mmu_v6::COARSE_DOMAIN:
		fsr |= 0xb;
		break;

	case arm_mmu_v6::SECTION_PERMISSION:
		fsr |= 0xd;
		break;

	case arm_mmu_v6::COARSE_PERMISSION:
		fsr |= 0xf;
		break;

	default:
		printf("mmu: unhandled resolution fault = %d\n", arm_fault);
		return false;
	}

	//printf("mmu: fault: l1=%08x fsr=%x far=%x arm-fault=%s type=%s:%s\n", l1->data, fsr, va, arm_faults[arm_fault], mem_access_modes[info.mode], mem_access_types[info.type]);

	if (likely(have_side_effects)) {
		*(armcpu().reg_offsets.FSR) = fsr;
		*(armcpu().reg_offsets.FAR) = va;
	}*/

	return true;
}

bool arm_mmu_v6::resolve_section(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, const l1_descriptor *l1)
{
#ifdef DEBUG_MMU
	printf("mmu-v6: resolving section for %08x\n", va);
#endif
	
	pa = l1->section.base_address() | (va & 0xfffff);
	return true;
}

bool arm_mmu_v6::resolve_coarse(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, const l1_descriptor *l1)
{
#ifdef DEBUG_MMU
	printf("mmu-v6: resolving coarse for %08x\n", va);
#endif
	
	const l2_descriptor *l2 = (const l2_descriptor *)resolve_guest_phys(l1->coarse_page_table.base_address());
	
#ifdef DEBUG_MMU
	printf("mmu-v6: resolved L2 @ %p data=%08x type=%d\n", l2, l2->data, l2->type());
#endif
	
	switch (l2->type()) {
	case l2_descriptor::SMALL_PAGE:
		pa = l2->small.base_address() | (va & 0xfff);
		break;
	default:
		return false;
	}
	
	return true;
}
