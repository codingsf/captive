#include <captive.h>
#include <engine/engine.h>
#include <loader/zimage-loader.h>
#include <loader/devtree-loader.h>
#include <hypervisor/config.h>
#include <hypervisor/cpu.h>
#include <hypervisor/kvm/kvm.h>

#include <devices/arm/pl011.h>
#include <devices/arm/sp810.h>

using namespace captive;
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

	// Attempt to create a guest in the hypervisor.
	GuestConfiguration cfg;
	cfg.name = "linux";

	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0, 0x10000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x20000000, 0x40000000));
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0x80000000, 0x60000000));

	devices::arm::PL011 *uart = new devices::arm::PL011();
	cfg.devices.push_back(GuestDeviceConfiguration(0x101f1000, 0x1000, *uart));

	devices::arm::SP810 *sysctl = new devices::arm::SP810();
	cfg.devices.push_back(GuestDeviceConfiguration(0x10000000, 0x1000, *sysctl));
	cfg.devices.push_back(GuestDeviceConfiguration(0x101e0000, 0x1000, *sysctl));

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

	if (!cpu->run()) {
		delete cpu;
		delete guest;
		delete hv;

		ERROR << "Unable to run CPU";
		return 1;
	}

	// Clean-up
	delete cpu;
	delete guest;
	delete hv;

	DEBUG << "Complete";

	return 0;
}
