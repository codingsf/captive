#include <captive.h>
#include <engine/test/test.h>
#include <hypervisor/config.h>
#include <hypervisor/cpu.h>
#include <hypervisor/kvm/kvm.h>

using namespace captive;
using namespace captive::engine;
using namespace captive::engine::test;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

int main(int argc, char **argv)
{
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
	cfg.name = "test";
	cfg.memory_regions.push_back(GuestMemoryRegionConfiguration(0, 0x40000000));	// 1Gb

	// Create the test engine.
	TestEngine engine;

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

	GuestCPUConfiguration cpu_cfg;

	CPU *cpu = guest->create_cpu(cpu_cfg);
	if (!cpu) {
		delete guest;
		delete hv;

		ERROR << "Unable to create CPU";
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
