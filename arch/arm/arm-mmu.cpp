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

bool arm_mmu_v6::resolve_gpa(const struct resolve_request& request, struct resolve_response& response, bool have_side_effects)
{
	arm_resolution_fault arm_fault = NONE;
	response.fault = arch::MMU::NONE;
	response.pa = 0xf0f0f0f0;
	
	if (!_enabled) {
		response.pa = request.va;
		return true;
	}

#ifdef DEBUG_MMU
	printf("--- ttbr0=%08x, ttbr1=%08x, ttbcr=%08x\n", *armcpu().reg_offsets.TTBR0, *armcpu().reg_offsets.TTBR1, *armcpu().reg_offsets.TTBCR);
	printf("mmu-v6: resolving %08x for %s in %s\n", va, mem_access_types[info.type], mem_access_modes[info.mode]);
#endif
	
	uint32_t ttbr = *armcpu().reg_offsets.TTBR0;
	
	//printf("mmu: TTBR=%08x, CTXID=%08x\n", ttbr, *armcpu().reg_offsets.CTXID);
		
	l1_descriptor *ttb = (l1_descriptor *)GPA_TO_HVA((gpa_t)(ttbr & ~0xfff));
	l1_descriptor *l1 = &ttb[request.va >> 20];
	
#ifdef DEBUG_MMU
	printf("mmu-v6: resolved L1 @ %p data=%08x type=%d\n", l1, l1->data, l1->type());
#endif
	
	uint32_t domain = 0;
	switch (l1->type()) {
	case l1_descriptor::SECTION:
		domain = l1->section.domain();
		if (!resolve_section(request, response, arm_fault, l1)) return false;
		break;
		
	case l1_descriptor::SUPERSECTION:
		fatal("mmu-v6: we do not support supersections\n");
		
	case l1_descriptor::COARSE_PAGE_TABLE:
		domain = l1->coarse_page_table.domain();
		if (!resolve_coarse(request, response, arm_fault, l1)) return false;
		break;
		
	case l1_descriptor::FAULT:
		arm_fault = SECTION_TRANSLATION;
		break;
	}

	if (arm_fault == NONE) {
#ifdef DEBUG_MMU
		printf("mmu-v6: resolved va=%08x => pa=%08x\n", va, pa);
#endif
		return true;
	}

#ifdef DEBUG_MMU
		printf("mmu-v6: fault va=%08x type=%d\n", va, arm_fault);
#endif

	switch (request.type) {
	case MMU::ACCESS_READ:
		response.fault = MMU::READ_FAULT;
		break;

	case MMU::ACCESS_WRITE:
		response.fault = MMU::WRITE_FAULT;
		break;

	case MMU::ACCESS_FETCH:
		response.fault = MMU::FETCH_FAULT;
		break;
	}

	if (likely(have_side_effects)) {
		if (response.fault == MMU::FETCH_FAULT) {
			*(armcpu().reg_offsets.IFAR) = request.va;
			*(armcpu().reg_offsets.IFSR) = (uint32_t)arm_fault;
		} else {
			uint32_t fsr = (((uint32_t)domain & 0xf) << 4) | (((uint32_t)arm_fault) & 0xf);
			if (response.fault == MMU::WRITE_FAULT) {
				fsr |= (1 << 11);
			}
			
			*(armcpu().reg_offsets.DFAR) = request.va;
			*(armcpu().reg_offsets.DFSR) = fsr;
		}
		
		#ifdef DEBUG_MMU
			printf("mmu-v6: fault va=%08x dfar=%08x ifar=%08x dfsr=%08x ifsr=%08x\n", va, *(armcpu().reg_offsets.DFAR), *(armcpu().reg_offsets.IFAR), *(armcpu().reg_offsets.DFSR), *(armcpu().reg_offsets.IFSR));
		#endif
	}

	return true;
}

bool arm_mmu_v6::check_access_perms(uint32_t ap, const struct resolve_request& request)
{
#ifdef DEBUG_MMU
	printf("mmu-v6: checking permissions %d kernel=%d, write=%d\n", ap, info.is_kernel(), info.is_write());
#endif
	
	switch(ap) {
	case 0: return false;
	case 1: return request.is_kernel();
	case 2: return request.is_kernel() || request.is_read() || request.is_fetch();
	case 3: return true;
	case 4: return false;
	case 5: return request.is_kernel() && (request.is_read() || request.is_fetch());
	case 6: return request.is_read() || request.is_fetch();
	case 7: return request.is_read() || request.is_fetch();

	default:
		printf("mmu: unknown access permission type %d\n", ap);
		return false;
	}
}

bool arm_mmu_v6::resolve_section(const struct resolve_request& request, struct resolve_response& response, arm_resolution_fault& fault, const l1_descriptor *l1)
{
#ifdef DEBUG_MMU
	printf("mmu-v6: resolving section for %08x\n", va);
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
		fault = SECTION_DOMAIN;
		return true;

	case 1:
#ifdef DEBUG_MMU
		printf("mmu-v6: AP=%d\n", l1->section.ap());
#endif

		if (!check_access_perms(l1->section.ap(), request)) {
			fault = SECTION_PERMISSION;
			return true;
		}
		break;
	}
	
	response.pa = l1->section.base_address() | (request.va & 0xfffff);
	return true;
}

bool arm_mmu_v6::resolve_coarse(const struct resolve_request& request, struct resolve_response& response, arm_resolution_fault& fault, const l1_descriptor *l1)
{
#ifdef DEBUG_MMU
	printf("mmu-v6: resolving coarse for %08x\n", va);
#endif
	
	const l2_descriptor *l2_base = (const l2_descriptor *)GPA_TO_HVA(l1->coarse_page_table.base_address());
	const l2_descriptor *l2 = &l2_base[(request.va >> 12) & 0xff];
	
	
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
		fault = PAGE_DOMAIN;
		return true;
	}
	
	switch (l2->type()) {
	case l2_descriptor::FAULT:
		fault = PAGE_TRANSLATION;
		return true;
		
	case l2_descriptor::SMALL_PAGE:
		if (dacr == 1) {
			if (!check_access_perms(l2->small.ap(), request)) {
				fault = PAGE_PERMISSION;
				return true;
			}
		}
		
		response.pa = l2->small.base_address() | (request.va & 0xfff);
		break;
		
	case l2_descriptor::LARGE_PAGE:
		if (dacr == 1) {
			if (!check_access_perms(l2->large.ap(), request)) {
				fault = PAGE_PERMISSION;
				return true;
			}
		}

		response.pa = l2->large.base_address() | (request.va & 0xffff);
		break;
		
	default:
		fatal("unsupported coarse page size\n");
	}
	
	return true;
}
