#include <hypervisor/cpu.h>
#include <hypervisor/config.h>
#include <captive.h>

USE_CONTEXT(Guest);
DECLARE_CHILD_CONTEXT(CPU, Guest);

using namespace captive::hypervisor;

CPU::CPU(Guest& owner, const GuestCPUConfiguration& config, PerCPUData& per_cpu_data) : _owner(owner), _config(config), _per_cpu_data(per_cpu_data)
{

}

CPU::~CPU()
{

}

bool CPU::init()
{
	return config().validate();
}
