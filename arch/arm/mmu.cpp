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

bool ArmMMU::resolve_gpa(gva_t va, gpa_t& pa) const
{
	if (!_enabled) {
		pa = va;
		return true;
	}

	pa = va;
	return true;
}