#include <captive.h>
#include <verify.h>
#include <engine/engine.h>
#include <jit/llvm.h>
#include <jit/wsj.h>
#include <loader/zimage-loader.h>
#include <loader/elf-loader.h>
#include <loader/devtree-loader.h>
#include <hypervisor/config.h>
#include <hypervisor/cpu.h>
#include <hypervisor/kvm/kvm.h>
#include <util/command-line.h>
#include <util/cl/options.h>

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
#include <devices/io/file-backed-block-device.h>
#include <devices/io/virtio/virtio-block-device.h>

#include <devices/gfx/sdl-virtual-screen.h>

#include <devices/timers/millisecond-tick-source.h>

DECLARE_CONTEXT(Main);

using namespace captive;
using namespace captive::devices::timers;
using namespace captive::engine;
using namespace captive::jit;
using namespace captive::loader;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;
using namespace captive::util;

int main(int argc, char **argv)
{
	const CommandLine *cl = CommandLine::parse(argc, argv);

	if (cl::Help) {
		return 1;
	}

	if (argc < 5 || argc > 7 || argc == 6) {
		ERROR << "usage: " << argv[0] << " <engine lib> <zimage> <device tree> <root fs> [--verify {0 | 1}]";
		return 1;
	}

	// Check that KVM is supported
	if (!KVM::supported()) {
		ERROR << "KVM is not supported";
		return 1;
	}

	if (argc == 7) {
		if (strcmp(argv[5], "--verify")) {
			ERROR << "usage: " << argv[0] << " <engine lib> <zimage> <device tree> <root fs> [--verify {0 | 1}]";
			return 1;
		}

		if (verify_prepare(atoi(argv[6]))) {
			ERROR << "Unable to prepare verification mode";
			return 1;
		}
	}

	// Create a new hypervisor.
	Hypervisor *hv = new KVM();
	if (!hv->init()) {
		ERROR << "Unable to initialise the hypervisor";
		return 1;
	}

	// Create the master tick source
	MillisecondTickSource mts;

	// Attempt to create a guest in the hypervisor.
	GuestConfiguration cfg;
	cfg.name = "linux";

	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0, 0x10000000));
	//cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x10000000, 0x100000000-0x10000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x20000000, 0x40000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x80000000, 0x60000000));

	devices::arm::ArmCpuIRQController *cpu_irq = new devices::arm::ArmCpuIRQController();
	cfg.cpu_irq_controller = cpu_irq;

	devices::arm::PL190 *vic = new devices::arm::PL190(*cpu_irq->get_irq_line(1), *cpu_irq->get_irq_line(0));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10140000, *vic));

	devices::arm::VersatileSIC *sic = new devices::arm::VersatileSIC(*vic->get_irq_line(31));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10003000, *sic));

	devices::arm::PL011 *uart0 = new devices::arm::PL011(*vic->get_irq_line(12));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f1000, *uart0));

	devices::arm::PL011 *uart1 = new devices::arm::PL011(*vic->get_irq_line(13));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f2000, *uart1));

	devices::arm::PL011 *uart2 = new devices::arm::PL011(*vic->get_irq_line(14));
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

	devices::arm::SP810 *sysctl = new devices::arm::SP810();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10000000, *sysctl));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e0000, *sysctl));

	devices::arm::SP804 *timer0 = new devices::arm::SP804(mts, *vic->get_irq_line(4));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e2000, *timer0));

	devices::arm::SP804 *timer1 = new devices::arm::SP804(mts, *vic->get_irq_line(5));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e3000, *timer1));

	devices::arm::PrimecellStub *static_memory_controller = new devices::arm::PrimecellStub(0x00141093, "smc", 0x10000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10100000, *static_memory_controller));

	devices::arm::PrimecellStub *mp_memory_controller = new devices::arm::PrimecellStub(0x47041175, "mpm", 0x10000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10110000, *mp_memory_controller));

	devices::arm::PrimecellStub *watchdog = new devices::arm::PrimecellStub(0x00141805, "watchdog", 0x1000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e1000, *watchdog));

	devices::arm::PrimecellStub *sci = new devices::arm::PrimecellStub(0x00041131, "sci", 0x1000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f0000, *sci));

	devices::arm::PrimecellStub *ssp = new devices::arm::PrimecellStub(0x00241022, "ssp", 0x1000);
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

	devices::io::FileBackedBlockDevice *bdev = new devices::io::FileBackedBlockDevice();

	if (!bdev->open_file(argv[4])) {
		ERROR << "Unable to open block device file '" << argv[4] << "'";
		return 1;
	}

	devices::io::virtio::VirtIOBlockDevice *vbd = new devices::io::virtio::VirtIOBlockDevice(*vic->get_irq_line(30), *bdev);
	cfg.devices.push_back(GuestDeviceConfiguration(0x11001000, *vbd));

	// Create the engine.
	Engine engine(argv[1]);
	if (!engine.init()) {
		delete hv;

		ERROR << "Unable to initialise engine";
		return 1;
	}

	// Create the JIT
	//LLVMJIT jit(engine);
	WSJ jit(engine);
	if (!jit.init()) {
		ERROR << "Unable to initialise jit";
		return 1;
	}

	Guest *guest = hv->create_guest(engine, (JIT&)jit, cfg);
	if (!guest) {
		delete hv;

		ERROR << "Unable to create guest VM";
		return 1;
	}

	// Initialise the guest
	if (!guest->init()) {
		delete guest;
		delete hv;

		ERROR << "Unable to initialise guest VM";
		return 1;
	}

	// Load the kernel
	ZImageLoader kernel(argv[2]);
	//ELFLoader kernel(argv[2]);
	if (!guest->load(kernel)) {
		delete guest;
		delete hv;

		ERROR << "Unable to load guest kernel";
		return 1;
	}

	guest->guest_entrypoint(kernel.entrypoint());

	// Load the device-tree
	DeviceTreeLoader device_tree(argv[3], 0x1000);
	if (!guest->load(device_tree)) {
		delete guest;
		delete hv;

		ERROR << "Unable to load device tree";
		return 1;
	}

	GuestCPUConfiguration cpu_cfg(GuestCPUConfiguration::RegionJIT); //(verify_enabled() && verify_get_tid() == 0) ? GuestCPUConfiguration::Interpreter : GuestCPUConfiguration::BlockJIT);

	CPU *cpu = guest->create_cpu(cpu_cfg);
	if (!cpu) {
		delete guest;
		delete hv;

		ERROR << "Unable to create CPU";
		return 1;
	}

	if (!cpu->init()) {
		delete cpu;
		delete guest;
		delete hv;

		ERROR << "Unable to initialise CPU";
		return 1;
	}

	vs->cpu(*cpu);

	// Start the tick source
	mts.start();

	if (!cpu->run()) {
		ERROR << "Unable to run CPU";
	}

	// Stop the tick source
	mts.stop();

	// Clean-up
	delete cpu;
	delete guest;
	delete hv;

	DEBUG << CONTEXT(Main) << "Complete";

	return 0;
}
