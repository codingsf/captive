#include <platform/realview.h>

#include <devices/arm/cpu-irq.h>
#include <devices/arm/gic.h>
#include <devices/arm/scu.h>
#include <devices/arm/mptimer.h>
#include <devices/arm/pl011.h>
#include <devices/arm/pl022.h>
#include <devices/arm/pl031.h>
#include <devices/arm/pl041.h>
#include <devices/arm/pl050.h>
#include <devices/arm/pl061.h>
#include <devices/arm/pl080.h>
#include <devices/arm/pl081.h>
#include <devices/arm/pl110.h>
#include <devices/arm/pl131.h>
#include <devices/arm/pl180.h>
#include <devices/arm/pl310.h>
#include <devices/arm/pl350.h>
#include <devices/arm/sp804.h>
#include <devices/arm/sp805.h>
#include <devices/arm/sp810.h>
#include <devices/arm/sbcon.h>
#include <devices/arm/primecell-stub.h>
#include <devices/arm/realview/system-status-and-control.h>
#include <devices/arm/realview/system-controller.h>

#include <devices/gfx/sdl-virtual-screen.h>

#include <devices/io/console-uart.h>
#include <devices/io/fd-uart.h>
#include <devices/io/null-uart.h>
#include <devices/io/socket-uart.h>
#include <devices/io/ps2.h>
#include <devices/io/file-backed-async-block-device.h>
#include <devices/io/virtio/virtio-block-device.h>


using namespace captive;
using namespace captive::hypervisor;
using namespace captive::platform;
using namespace captive::devices;
using namespace captive::devices::arm;
using namespace captive::devices::arm::realview;
using namespace captive::devices::gfx;
using namespace captive::devices::io;
using namespace captive::devices::io::virtio;

Realview::Realview(devices::timers::TickSource& ts, std::string block_device_file) : socket_uart(NULL)
{
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0, 0x10000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x20000000, 0x20000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x40000000, 0x20000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x70000000, 0x20000000));
	
	SystemStatusAndControl *statctl = new SystemStatusAndControl(ts);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10000000, *statctl));

	SystemController *syscon0 = new SystemController(SystemController::SYS_CTRL0);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10001000, *syscon0));

	SystemController *syscon1 = new SystemController(SystemController::SYS_CTRL1);
	cfg.devices.push_back(GuestDeviceConfiguration(0x1001A000, *syscon1));
	
	GuestCPUConfiguration core0(*new ArmCpuIRQController());
	GuestCPUConfiguration core1(*new ArmCpuIRQController());

	cfg.cores.push_back(core0);	
	cfg.cores.push_back(core1);	
	
	SnoopControlUnit *scu = new SnoopControlUnit();
	cfg.devices.push_back(GuestDeviceConfiguration(0x1f000000, *scu));
	
	PL310 *pl310 = new PL310();
	cfg.devices.push_back(GuestDeviceConfiguration(0x1f002000, *pl310));

	GIC *gic0 = new GIC(*((ArmCpuIRQController&)core0.cpu_irq_controller()).get_irq_line(1), *((ArmCpuIRQController&)core1.cpu_irq_controller()).get_irq_line(1));
	
	cfg.devices.push_back(GuestDeviceConfiguration(0x1f000100, gic0->get_cpu(0)));
	cfg.devices.push_back(GuestDeviceConfiguration(0x1f001000, gic0->get_distributor()));

	MPTimer *mpt = new MPTimer(ts, *gic0->get_irq_line(29));
	cfg.devices.push_back(GuestDeviceConfiguration(0x1f000600, *mpt));
	
	SP804 *timer0 = new SP804(ts, *gic0->get_irq_line(36));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10011000, *timer0));

	SP804 *timer1 = new SP804(ts, *gic0->get_irq_line(37));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10012000, *timer1));
	
	SP804 *timer2 = new SP804(ts, *gic0->get_irq_line(73));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10018000, *timer2));
	
	SP804 *timer3 = new SP804(ts, *gic0->get_irq_line(74));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10019000, *timer3));
	
	SP805 *wdog0 = new SP805();
	cfg.devices.push_back(GuestDeviceConfiguration(0x1000f000, *wdog0));

	SP805 *wdog1 = new SP805();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10010000, *wdog1));

	PL061 *gpio0 = new PL061(*gic0->get_irq_line(38));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10013000, *gpio0));

	PL061 *gpio1 = new PL061(*gic0->get_irq_line(39));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10014000, *gpio1));
	
	PL061 *gpio2 = new PL061(*gic0->get_irq_line(40));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10015000, *gpio2));
	
	PL031 *rtc = new PL031();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10017000, *rtc));
	
	PL041 *aaci = new PL041();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10004000, *aaci));

	PL022 *ssp = new PL022();
	cfg.devices.push_back(GuestDeviceConfiguration(0x1000d000, *ssp));
	
	auto console = new devices::io::ConsoleUART();
	//console->open();
	
	uart0 = new devices::arm::PL011(*gic0->get_irq_line(44), *console);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10009000, *uart0));

	/*socket_uart = new devices::io::SocketUART(9233);
	if (!socket_uart->open()) {
		ERROR << "Unable to create UART socket";
		throw 0;
	}*/
	
	uart1 = new devices::arm::PL011(*gic0->get_irq_line(45), *new devices::io::NullUART());
	cfg.devices.push_back(GuestDeviceConfiguration(0x1000a000, *uart1));
	
	uart2 = new devices::arm::PL011(*gic0->get_irq_line(46), *new devices::io::NullUART());
	cfg.devices.push_back(GuestDeviceConfiguration(0x1000b000, *uart2));
	
	uart3 = new devices::arm::PL011(*gic0->get_irq_line(47), *new devices::io::NullUART());
	cfg.devices.push_back(GuestDeviceConfiguration(0x1000c000, *uart3));
	
	PL081 *dma = new PL081();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10030000, *dma));

	PL131 *sci = new PL131();
	cfg.devices.push_back(GuestDeviceConfiguration(0x1000e000, *sci));

	PL180 *mmc = new PL180();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10005000, *mmc));

	PL350 *smc = new PL350();
	cfg.devices.push_back(GuestDeviceConfiguration(0x100e1000, *smc));
	
	SBCON *sbcon0 = new SBCON();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10002000, *sbcon0));
	
	PS2KeyboardDevice *ps2kbd = new PS2KeyboardDevice(*gic0->get_irq_line(52));
	PS2MouseDevice *ps2mse = new PS2MouseDevice(*gic0->get_irq_line(53));

	PL050 *kbd = new devices::arm::PL050(*ps2kbd);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10006000, *kbd));

	PL050 *mse = new devices::arm::PL050(*ps2mse);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10007000, *mse));

	vs = new SDLVirtualScreen();
	vs->keyboard(*ps2kbd);
	vs->mouse(*ps2mse);
	
	PL110 *lcd = new PL110(*vs, *gic0->get_irq_line(55), PL110::V_PL111);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10020000, *lcd));
	
	FileBackedAsyncBlockDevice *bdev = new FileBackedAsyncBlockDevice();

	if (!bdev->open_file(block_device_file)) {
		ERROR << "Unable to open block device file '" << block_device_file << "'";
		throw 0;
	}

	VirtIOBlockDevice *vbd = new VirtIOBlockDevice(*gic0->get_irq_line(35), *bdev);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10100000, *vbd));
}

Realview::~Realview()
{
}

bool Realview::start()
{
	uart0->start_reading();
	uart1->start_reading();

	//vs->cpu(*cores().front());
	return true;
}

bool Realview::stop()
{
	uart1->stop_reading();
	uart0->stop_reading();
	
	if (socket_uart)
		socket_uart->close();
	return true;
}

const hypervisor::GuestConfiguration& Realview::config() const
{
	return cfg;
}
