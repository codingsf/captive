#include <arm-mmu.h>
#include <arm-cpu.h>
#include <arm-env.h>
#include <printf.h>

#include "include/arm-mmu.h"

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

bool ArmMMU::resolve_gpa(gva_t va, gpa_t& pa) const
{
	if (!_enabled) {
		pa = va;
		return true;
	}

	printf("mmu: resolve va=%08x, ttbr0=%x\n", va, _coco.TTBR0());

	// Acquire the actual TTBR
	tt_base ttbr = (tt_base)get_ttbr();
	if (!ttbr) {
		return false;
	}

	// Calculate the ttbr index
	uint16_t ttbr_idx = va >> 20;
	printf("ttb entry: type=%d, addr=%x\n", ttbr[ttbr_idx].type(), ttbr[ttbr_idx].base_addr());

	if (ttbr[ttbr_idx].type() == tt_entry::TT_ENTRY_FAULT) {
		return false;
	}

	return false;
}