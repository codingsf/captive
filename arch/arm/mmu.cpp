#include <arm-mmu.h>
#include <arm-cpu.h>
#include <printf.h>

#include "include/arm-mmu.h"

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
	return true;
}

bool ArmMMU::disable()
{
	if (!_enabled) return true;

	install_phys_vma();

	_enabled = false;
	return true;
}

