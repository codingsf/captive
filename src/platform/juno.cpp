#include <platform/juno.h>
#include <devices/arm/cpu-irq.h>

using namespace captive;
using namespace captive::hypervisor;
using namespace captive::platform;
using namespace captive::devices::arm;

Juno::Juno(const util::config::Configuration& hcfg, devices::timers::TimerManager& timer_manager, Variant variant) : Platform(hcfg, timer_manager), variant(variant), cfg()
{
	cfg.memory_regions.clear();
	
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0, 0x1000000));
	
	ArmCpuIRQController *core0irq = new ArmCpuIRQController();
	GuestCPUConfiguration core0(*core0irq);
	cfg.cores.push_back(core0);
}

Juno::~Juno() {

}

bool Juno::start()
{
	return true;
}

bool Juno::stop()
{
	return true;
}

void Juno::dump() const
{

}
