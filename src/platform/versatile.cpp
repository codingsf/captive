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

#include <devices/io/keyboard.h>
#include <devices/io/mouse.h>
#include <devices/io/ps2.h>
#include <devices/io/null-uart.h>
#include <devices/io/console-uart.h>
#include <devices/io/file-backed-async-block-device.h>
#include <devices/io/virtio/virtio-block-device.h>

#include <devices/gfx/sdl-virtual-screen.h>

#include <devices/timers/microsecond-tick-source.h>
#include <devices/timers/callback-tick-source.h>

#include <socket.h>

#if 0
static devices::io::UART *create_socket_uart(std::string name)
{
	UnixDomainSocket *socket = new UnixDomainSocket("/tmp/" + name + ".sock");
	if (!socket->create()) {
		if (name == "uart0")
			return new devices::io::ConsoleUART();
		else
			return new devices::io::NullUART();
	}

	if (socket->connect()) {
		return new devices::io::FDUART(socket->get_fd(), socket->get_fd());
	} else {
		if (name == "uart0")
			return new devices::io::ConsoleUART();
		else
			return new devices::io::NullUART();
	}
}

cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0, 0x10000000));
	//cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x10000000, 0x100000000-0x10000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x20000000, 0x40000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x60000000, 0x10000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x80000000, 0x60000000));

	devices::arm::ArmCpuIRQController *cpu_irq = new devices::arm::ArmCpuIRQController();
	cfg.cpu_irq_controller = cpu_irq;

	devices::arm::PL190 *vic = new devices::arm::PL190(*cpu_irq->get_irq_line(1), *cpu_irq->get_irq_line(0));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10140000, *vic));

	devices::arm::VersatileSIC *sic = new devices::arm::VersatileSIC(*vic->get_irq_line(31));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10003000, *sic));

	devices::arm::PL011 *uart0 = new devices::arm::PL011(*vic->get_irq_line(12), *create_socket_uart("uart0"));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f1000, *uart0));

	devices::arm::PL011 *uart1 = new devices::arm::PL011(*vic->get_irq_line(13), *create_socket_uart("uart1"));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f2000, *uart1));

	devices::arm::PL011 *uart2 = new devices::arm::PL011(*vic->get_irq_line(14), *create_socket_uart("uart2"));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f3000, *uart2));

	devices::arm::PL031 *rtc = new devices::arm::PL031();
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e8000, *rtc));

	devices::io::PS2KeyboardDevice *ps2kbd = new devices::io::PS2KeyboardDevice(*sic->get_irq_line(3));
	devices::io::PS2MouseDevice *ps2mse = new devices::io::PS2MouseDevice(*sic->get_irq_line(4));

	devices::arm::PL050 *kbd = new devices::arm::PL050(*ps2kbd);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10006000, *kbd));

	devices::arm::PL050 *mse = new devices::arm::PL050(*ps2mse);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10007000, *mse));

	devices::arm::PL080 *dma = new devices::arm::PL080();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10130000, *dma));

	devices::gfx::SDLVirtualScreen *vs = new devices::gfx::SDLVirtualScreen();
	vs->keyboard(*ps2kbd);
	vs->mouse(*ps2mse);

	devices::arm::PL110 *lcd = new devices::arm::PL110(*vs, *vic->get_irq_line(16));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10120000, *lcd));

	devices::arm::SP810 *sysctl = new devices::arm::SP810(*ts);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10000000, *sysctl));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e0000, *sysctl));

	devices::arm::SP804 *timer0 = new devices::arm::SP804(*ts, *vic->get_irq_line(4));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e2000, *timer0));

	devices::arm::SP804 *timer1 = new devices::arm::SP804(*ts, *vic->get_irq_line(5));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e3000, *timer1));

	devices::arm::PrimecellStub *static_memory_controller = new devices::arm::PrimecellStub(0x93101400, "smc", 0x10000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10100000, *static_memory_controller));

	devices::arm::PrimecellStub *mp_memory_controller = new devices::arm::PrimecellStub(0x75110447, "mpm", 0x10000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10110000, *mp_memory_controller));

	devices::arm::PrimecellStub *watchdog = new devices::arm::PrimecellStub(0x05181400, "watchdog", 0x1000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e1000, *watchdog));

	devices::arm::PrimecellStub *sci = new devices::arm::PrimecellStub(0x31110400, "sci", 0x1000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f0000, *sci));

	devices::arm::PrimecellStub *ssp = new devices::arm::PrimecellStub(0x22102400, "ssp", 0x1000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f4000, *ssp));

	devices::arm::PrimecellStub *net = new devices::arm::PrimecellStub(0xf0f0f0f0, "net", 0x10000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10010000, *net));

	devices::arm::PrimecellStub *aaci = new devices::arm::PrimecellStub(0xf0f0f0f0, "aaci", 0x1000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10004000, *aaci));

	devices::arm::PrimecellStub *mci0 = new devices::arm::PrimecellStub(0xf0f0f0f0, "mci", 0x1000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10005000, *mci0));

	devices::arm::PL061 *gpio0 = new devices::arm::PL061(*vic->get_irq_line(6));
	devices::arm::PL061 *gpio1 = new devices::arm::PL061(*vic->get_irq_line(7));
	devices::arm::PL061 *gpio2 = new devices::arm::PL061(*vic->get_irq_line(8));
	devices::arm::PL061 *gpio3 = new devices::arm::PL061(*vic->get_irq_line(9));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e4000, *gpio0));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e5000, *gpio1));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e6000, *gpio2));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e7000, *gpio3));

	devices::io::FileBackedAsyncBlockDevice *bdev = new devices::io::FileBackedAsyncBlockDevice();

	DEBUG << "Opening file " << std::string(argv[4]) << " for block device";
	if (!bdev->open_file(argv[4])) {
		ERROR << "Unable to open block device file '" << argv[4] << "'";
		return 1;
	}

	devices::io::virtio::VirtIOBlockDevice *vbd = new devices::io::virtio::VirtIOBlockDevice(*vic->get_irq_line(30), *bdev);
	cfg.devices.push_back(GuestDeviceConfiguration(0x11001000, *vbd));
#endif