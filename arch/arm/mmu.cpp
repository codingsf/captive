#include <arm-mmu.h>
#include <arm-cpu.h>
#include <printf.h>

using namespace captive::arch::arm;

ArmMMU::ArmMMU(ArmCPU& cpu) : MMU(cpu.env()), _cpu(cpu)
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

	install_phys_vma();

	_enabled = false;
	printf("mmu: disabled\n");
	return true;
}

bool ArmMMU::handle_fault(uint64_t va)
{
	pm_t pm;
	pdp_t pdp;
	pd_t pd;
	pt_t pt;

	printf("mmu: (%s) handle fault va=%x\n", _enabled ? "enabled" : "disabled", va);

	va_entries(va, pm, pdp, pd, pt);
	printf("mmu: pm=%p, pdp=%p, pd=%p, pt=%p (%x)\n", pm, pdp, pd, pt, *pt);

	if (!_enabled) {
		return false;
	}

	return true;
}
