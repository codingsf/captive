#include <platform/realview.h>

#include <devices/arm/cpu-irq.h>
#include <devices/arm/pl011.h>
#include <devices/arm/pl031.h>
#include <devices/arm/pl050.h>
#include <devices/arm/pl061.h>
#include <devices/arm/pl080.h>
#include <devices/arm/pl110.h>
#include <devices/arm/pl190.h>
#include <devices/arm/sp804.h>
#include <devices/arm/sp810.h>
#include <devices/arm/versatile-sic.h>
#include <devices/arm/primecell-stub.h>

#include <devices/io/console-uart.h>

using namespace captive;
using namespace captive::hypervisor;
using namespace captive::platform;

Realview::Realview()
{
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0, 0x08000000));
	
	devices::arm::ArmCpuIRQController *cpu_irq = new devices::arm::ArmCpuIRQController();
	cfg.cpu_irq_controller = cpu_irq;
	
	devices::arm::PL190 *vic = new devices::arm::PL190(*cpu_irq->get_irq_line(1), *cpu_irq->get_irq_line(0));
	cfg.devices.push_back(GuestDeviceConfiguration(0xf0000000, *vic));

	uart0 = new devices::arm::PL011(*vic->get_irq_line(12), *new devices::io::ConsoleUART());
	cfg.devices.push_back(GuestDeviceConfiguration(0x1010c000, *uart0));
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
