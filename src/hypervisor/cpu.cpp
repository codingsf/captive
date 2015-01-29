#include <hypervisor/cpu.h>

#include "hypervisor/config.h"

using namespace captive::hypervisor;

CPU::CPU(Guest& owner, const GuestCPUConfiguration& config) : _owner(owner), _config(config)
{
	
}

CPU::~CPU()
{
	
}

bool CPU::init()
{
	return config().validate();
}
