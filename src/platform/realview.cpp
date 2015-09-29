#include <platform/realview.h>

#include <devices/arm/cpu-irq.h>
#include <devices/arm/gic.h>
#include <devices/arm/pl011.h>
#include <devices/arm/pl031.h>
#include <devices/arm/pl050.h>
#include <devices/arm/pl061.h>
#include <devices/arm/pl080.h>
#include <devices/arm/pl110.h>
#include <devices/arm/sp804.h>
#include <devices/arm/sp810.h>
#include <devices/arm/versatile-sic.h>
#include <devices/arm/primecell-stub.h>
#include <devices/arm/realview/sysctl.h>

#include <devices/io/console-uart.h>

using namespace captive;
using namespace captive::hypervisor;
using namespace captive::platform;
using namespace captive::devices;
using namespace captive::devices::arm;
using namespace captive::devices::arm::realview;

Realview::Realview(devices::timers::TickSource& ts)
{
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0, 0x10000000));
	
	SysCtl *sysctl = new SysCtl(ts);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10000000, *sysctl));
	
	ArmCpuIRQController *cpu_irq = new ArmCpuIRQController();
	cfg.cpu_irq_controller = cpu_irq;
	
	GIC *gic0 = new GIC(*cpu_irq->get_irq_line(1));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10121000, *gic0));
	
	SP804 *timer0 = new SP804(ts, *gic0->get_irq_line(8));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10011000, *timer0));

	SP804 *timer1 = new SP804(ts, *gic0->get_irq_line(10));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10018000, *timer1));
	
	SP804 *timer2 = new SP804(ts, *gic0->get_irq_line(10));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10019000, *timer1));
	
	SP804 *timer3 = new SP804(ts, *gic0->get_irq_line(10));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10012000, *timer1));

	uart0 = new devices::arm::PL011(*gic0->get_irq_line(18), *new devices::io::ConsoleUART());
	cfg.devices.push_back(GuestDeviceConfiguration(0x10009000, *uart0));
}

Realview::~Realview() {

}

bool Realview::start()
{
	uart0->start_reading();
	return true;
}

bool Realview::stop()
{
	uart0->stop_reading();
	return true;
}

const hypervisor::GuestConfiguration& Realview::config() const
{
	return cfg;
}
