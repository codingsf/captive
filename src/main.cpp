#include <captive.h>
#include <engine/engine.h>
#include <loader/zimage-loader.h>
#include <loader/devtree-loader.h>
#include <hypervisor/config.h>
#include <hypervisor/cpu.h>
#include <hypervisor/kvm/kvm.h>

#include <devices/arm/cpu-irq.h>
#include <devices/arm/pl011.h>
#include <devices/arm/pl080.h>
#include <devices/arm/pl110.h>
#include <devices/arm/pl190.h>
#include <devices/arm/sp804.h>
#include <devices/arm/sp810.h>
#include <devices/arm/versatile-sic.h>
#include <devices/arm/primecell-stub.h>

#include <devices/timers/millisecond-tick-source.h>

using namespace captive;
using namespace captive::devices::timers;
using namespace captive::engine;
using namespace captive::loader;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

int main(int argc, char **argv)
{
	if (argc != 4) {
		ERROR << "usage: " << argv[0] << " <engine lib> <zimage> <device tree>";
		return 1;
	}

	// Check that KVM is supported
	if (!KVM::supported()) {
		ERROR << "KVM is not supported";
		return 1;
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
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x20000000, 0x40000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x80000000, 0x60000000));

	devices::arm::ArmCpuIRQController *cpu_irq = new devices::arm::ArmCpuIRQController();
	cfg.cpu_irq_controller = cpu_irq;

	devices::arm::PL011 *uart0 = new devices::arm::PL011();
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f1000, *uart0));

	devices::arm::PL011 *uart1 = new devices::arm::PL011();
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f2000, *uart1));

	devices::arm::PL011 *uart2 = new devices::arm::PL011();
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f3000, *uart2));

	devices::arm::PL080 *dma = new devices::arm::PL080();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10130000, *dma));

	devices::arm::PL110 *lcd = new devices::arm::PL110();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10120000, *lcd));

	devices::arm::SP810 *sysctl = new devices::arm::SP810();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10000000, *sysctl));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e0000, *sysctl));

	devices::arm::PL190 *vic = new devices::arm::PL190(*cpu_irq->get_irq_line(0), *cpu_irq->get_irq_line(1));
	cfg.devices.push_back(GuestDeviceConfiguration(0x10140000, *vic));

	devices::arm::VersatileSIC *sic = new devices::arm::VersatileSIC();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10003000, *sic));

	devices::arm::SP804 *timer0 = new devices::arm::SP804(mts, *vic->get_irq_line(4));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e2000, *timer0));

	devices::arm::SP804 *timer1 = new devices::arm::SP804(mts, *vic->get_irq_line(5));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e3000, *timer1));

	devices::arm::PrimecellStub *static_memory_controller = new devices::arm::PrimecellStub(0x00141093, 0x10000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10100000, *static_memory_controller));

	devices::arm::PrimecellStub *mp_memory_controller = new devices::arm::PrimecellStub(0x47041175, 0x10000);
	cfg.devices.push_back(GuestDeviceConfiguration(0x10110000, *mp_memory_controller));


	// Create the engine.
	Engine engine(argv[1]);

	if (!engine.init()) {
		delete hv;

		ERROR << "Unable to initialise engine";
		return 1;
	}

	Guest *guest = hv->create_guest(engine, cfg);
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

	// Load the zimage
	ZImageLoader zimage(argv[2]);
	if (!guest->load(zimage)) {
		delete guest;
		delete hv;

		ERROR << "Unable to load ZIMAGE";
		return 1;
	}

	guest->guest_entrypoint(zimage.entrypoint());

	// Load the device-tree
	DeviceTreeLoader device_tree(argv[3], 0x1000);
	if (!guest->load(device_tree)) {
		delete guest;
		delete hv;

		ERROR << "Unable to load device tree";
		return 1;
	}

	GuestCPUConfiguration cpu_cfg;

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

	// Start the tick source
	mts.start();

	if (!cpu->run()) {
		delete cpu;
		delete guest;
		delete hv;

		ERROR << "Unable to run CPU";
		return 1;
	}

	// Stop the tick source
	mts.stop();

	// Clean-up
	delete cpu;
	delete guest;
	delete hv;

	DEBUG << "Complete";

	return 0;
}
