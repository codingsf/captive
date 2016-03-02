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

arm_mmu::arm_mmu(arm_cpu& cpu) : MMU(cpu), _coco((devices::CoCo&)*cpu.env().lookup_core_device(15)), _enabled(false)
{

}

arm_mmu::~arm_mmu()
{

}

MMU *arm_mmu::create(arm_cpu& cpu)
{
	if (((arm_environment &)cpu.env()).arch_variant() == arm_environment::ARMv5)
		return NULL;
	else
		return new arm_mmu_v6(cpu);
}

bool arm_mmu::enable()
{
	if (_enabled) return true;

#ifdef DEBUG_MMU
	printf("mmu: enable\n");
#endif
	
	invalidate_virtual_mappings();
	cpu().invalidate_translations();
	
	_enabled = true;
	return true;
}

bool arm_mmu::disable()
{
	if (!_enabled) return true;
	
#ifdef DEBUG_MMU
	printf("mmu: disable %x\n", cpu().read_pc());
#endif

	invalidate_virtual_mappings();
	cpu().invalidate_translations();

	_enabled = false;
	return true;
}

arm_mmu_v6::arm_mmu_v6(arm_cpu& cpu) : arm_mmu(cpu)
{

}

arm_mmu_v6::~arm_mmu_v6()
{

}

bool arm_mmu_v6::resolve_gpa(struct resolution_context& rc, bool have_side_effects)
{
	arm_resolution_fault arm_fault = NONE;
	rc.fault = arch::MMU::NONE;
	rc.pa = 0xf0f0f0f0;
	rc.allowed_permissions = NO_PERMISSION;

	if (!_enabled) {
		rc.pa = rc.va;
		return true;
	}

#ifdef DEBUG_MMU
	printf("--- ttbr0=%08x, ttbr1=%08x, ttbcr=%08x\n", *armcpu().reg_offsets.TTBR0, *armcpu().reg_offsets.TTBR1, *armcpu().reg_offsets.TTBCR);
	printf("mmu-v6: resolving %08x for perms=%x\n", rc.va, rc.requested_permissions);
#endif
	
	//printf("mmu: va=%08x, ttbr0=%08x, ttbr1=%08x, ttbcr=%08x, ctxid=%x\n", rc.va, (*armcpu().reg_offsets.TTBR0), (*armcpu().reg_offsets.TTBR1), (*armcpu().reg_offsets.TTBCR), context_id());
	
	uint32_t ttbr = (*armcpu().reg_offsets.TTBR0) & ~0xfff;
	
	l1_descriptor *ttb = (l1_descriptor *)GPA_TO_HVA((gpa_t)ttbr);
	l1_descriptor *l1 = &ttb[rc.va >> 20];
	
#ifdef DEBUG_MMU
	printf("mmu-v6: resolved L1 @ %p data=%08x type=%d\n", l1, l1->data, l1->type());
#endif
	
	uint32_t domain = 0;
	switch (l1->type()) {
	case l1_descriptor::SECTION:
		domain = l1->section.domain();
		if (!resolve_section(rc, arm_fault, l1)) return false;
		break;
		
	case l1_descriptor::SUPERSECTION:
		fatal("mmu-v6: we do not support supersections\n");
		
	case l1_descriptor::COARSE_PAGE_TABLE:
		domain = l1->coarse_page_table.domain();
		if (!resolve_coarse(rc, arm_fault, l1)) return false;
		break;
		
	case l1_descriptor::FAULT:
		arm_fault = SECTION_TRANSLATION;
		break;
	}

	if (arm_fault == NONE) {
#ifdef DEBUG_MMU
		printf("mmu-v6: resolved va=%08x => pa=%08x (aperms = %x)\n", rc.va, rc.pa, rc.allowed_permissions);
#endif
		return true;
	}

#ifdef DEBUG_MMU
		printf("mmu-v6: fault va=%08x type=%d\n", rc.va, arm_fault);
#endif

	switch (rc.requested_permissions) {
	case MMU::USER_READ:
	case MMU::KERNEL_READ:
		rc.fault = MMU::READ_FAULT;
		break;

	case MMU::USER_WRITE:
	case MMU::KERNEL_WRITE:
		rc.fault = MMU::WRITE_FAULT;
		break;

	case MMU::USER_FETCH:
	case MMU::KERNEL_FETCH:
		rc.fault = MMU::FETCH_FAULT;
		break;
		
	default:
		fatal("unsupported requested permission combination\n");
	}

	if (likely(have_side_effects)) {
		if (rc.fault == MMU::FETCH_FAULT) {
			*(armcpu().reg_offsets.IFAR) = rc.va;
			*(armcpu().reg_offsets.IFSR) = (uint32_t)arm_fault;
		} else {
			uint32_t fsr = (((uint32_t)domain & 0xf) << 4) | (((uint32_t)arm_fault) & 0xf);
			if (rc.fault == MMU::WRITE_FAULT) {
				fsr |= (1 << 11);
			}
			
			*(armcpu().reg_offsets.DFAR) = rc.va;
			*(armcpu().reg_offsets.DFSR) = fsr;
		}
		
		#ifdef DEBUG_MMU
			printf("mmu-v6: fault va=%08x dfar=%08x ifar=%08x dfsr=%08x ifsr=%08x\n", rc.va, *(armcpu().reg_offsets.DFAR), *(armcpu().reg_offsets.IFAR), *(armcpu().reg_offsets.DFSR), *(armcpu().reg_offsets.IFSR));
		#endif
	}

	return true;
}

bool arm_mmu_v6::update_permissions(uint32_t ap, struct resolution_context& rc)
{
	switch(ap) {
	case 0: rc.allowed_permissions = NO_PERMISSION; break;
	case 1: rc.allowed_permissions = KERNEL_ALL; break;
	case 2: rc.allowed_permissions = (enum permissions)(KERNEL_ALL | USER_READ_FETCH); break;
	case 3: rc.allowed_permissions = ALL_READ_WRITE_FETCH; break;
	case 4: rc.allowed_permissions = NO_PERMISSION; break;
	case 5: rc.allowed_permissions = KERNEL_READ_FETCH; break;
	case 6: rc.allowed_permissions = ALL_READ_FETCH; break;
	case 7: rc.allowed_permissions = ALL_READ_FETCH; break;

	default:
		printf("mmu: unknown access permission type %d\n", ap);
		return false;
	}

#ifdef DEBUG_MMU
	printf("mmu-v6: updating permissions: aperms=%x\n", rc.allowed_permissions);
#endif	
	return true;
}

bool arm_mmu_v6::resolve_section(struct resolution_context& rc, arm_resolution_fault& fault, const l1_descriptor *l1)
{
#ifdef DEBUG_MMU
	printf("mmu-v6: resolving section for %08x\n", rc.va);
#endif
	
	uint32_t dacr = *(armcpu().reg_offsets.DACR);
	dacr >>= l1->section.domain() * 2;
	dacr &= 0x3;
	
#ifdef DEBUG_MMU
	printf("mmu-v6: section domain access permissions: %x\n", dacr);
#endif
	
	switch (dacr) {
	case 0:
	case 2:
		rc.allowed_permissions = NO_PERMISSION;
		fault = SECTION_DOMAIN;
		return true;

	case 1:
#ifdef DEBUG_MMU
		printf("mmu-v6: AP=%d\n", l1->section.ap());
#endif

		update_permissions(l1->section.ap(), rc);
		if (!permit_access(rc)) {
			fault = SECTION_PERMISSION;
			return true;
		}
		break;
		
	case 3:
		rc.allowed_permissions = ALL_READ_WRITE_FETCH;
		break;
	}
	
	rc.pa = l1->section.base_address() | (rc.va & 0xfffff);
	return true;
}

bool arm_mmu_v6::resolve_coarse(struct resolution_context& rc, arm_resolution_fault& fault, const l1_descriptor *l1)
{
#ifdef DEBUG_MMU
	printf("mmu-v6: resolving coarse for %08x\n", rc.va);
#endif
	
	const l2_descriptor *l2_base = (const l2_descriptor *)GPA_TO_HVA(l1->coarse_page_table.base_address());
	const l2_descriptor *l2 = &l2_base[(rc.va >> 12) & 0xff];
	
	
#ifdef DEBUG_MMU
	printf("mmu-v6: resolved L2 @ %p data=%08x type=%d\n", l2, l2->data, l2->type());
#endif

	uint32_t dacr = *(armcpu().reg_offsets.DACR);
	dacr >>= l1->coarse_page_table.domain() * 2;
	dacr &= 0x3;
	
#ifdef DEBUG_MMU
	printf("mmu-v6: coarse domain access permissions: %x\n", dacr);
#endif

	switch (dacr) {
	case 0:
	case 2:
		rc.allowed_permissions = NO_PERMISSION;
		fault = PAGE_DOMAIN;
		return true;
	}
	
	switch (l2->type()) {
	case l2_descriptor::FAULT:
		rc.allowed_permissions = NO_PERMISSION;
		fault = PAGE_TRANSLATION;
		return true;
		
	case l2_descriptor::SMALL_PAGE:
		if (dacr == 1) {
			update_permissions(l2->small.ap(), rc);
			if (!permit_access(rc)) {
				fault = PAGE_PERMISSION;
				return true;
			}
		} else {
			rc.allowed_permissions = ALL_READ_WRITE_FETCH;
		}
		
		rc.pa = l2->small.base_address() | (rc.va & 0xfff);
		break;
		
	case l2_descriptor::LARGE_PAGE:
		if (dacr == 1) {
			update_permissions(l2->large.ap(), rc);
			if (!permit_access(rc)) {
				fault = PAGE_PERMISSION;
				return true;
			}
		} else {
			rc.allowed_permissions = ALL_READ_WRITE_FETCH;
		}

		rc.pa = l2->large.base_address() | (rc.va & 0xffff);
		break;
		
	default:
		fatal("unsupported coarse page size\n");
	}
	
	return true;
}
