#include <platform/gensim-test.h>
#include <devices/arm/cpu-irq.h>
#include <devices/arm/pl190.h>
#include <devices/arm/pl011.h>
#include <devices/arm/sp804.h>
#include <devices/io/console-uart.h>

using namespace captive;
using namespace captive::platform;
using namespace captive::hypervisor;
using namespace captive::devices;
using namespace captive::devices::arm;
using namespace captive::devices::io;

GensimTest::GensimTest(devices::timers::TimerManager& timer_manager) : Platform(timer_manager)
{
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0, 0x10000000));
	
	ArmCpuIRQController *core0irq = new ArmCpuIRQController();
	GuestCPUConfiguration core0(*core0irq);
	cfg.cores.push_back(core0);
	
	// PL190 10140000
	// PL011 101f1000
	// SP804 101e2000
	// SP804 101e3000
	
	PL190 *pl190 = new PL190(*core0irq->get_irq_line(1), *core0irq->get_irq_line(0));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10140000, *pl190));
	
	ConsoleUART *console = new ConsoleUART();
	PL011 *pl011 = new PL011(*pl190->get_irq_line(30), *console);
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f1000, *pl011));
}

GensimTest::~GensimTest()
{
}

bool GensimTest::start()
{
	return true;
}

bool GensimTest::stop()
{
	return true;
}

const hypervisor::GuestConfiguration& GensimTest::config() const
{
	return cfg;
}
