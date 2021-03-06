#include <captive.h>
#include <engine/engine.h>

#include <loader/zimage-loader.h>
#include <loader/elf-loader.h>
#include <loader/devtree-loader.h>
#include <loader/atags-loader.h>
#include <loader/initrd-loader.h>

#include <hypervisor/config.h>
#include <hypervisor/cpu.h>
#include <hypervisor/kvm/kvm.h>

#include <platform/realview.h>

#include <util/command-line.h>
#include <util/thread-pool.h>
#include <util/cl/options.h>

#include <devices/timers/callback-tick-source.h>
#include <devices/timers/microsecond-tick-source.h>

DECLARE_CONTEXT(Main);

using namespace captive;
using namespace captive::engine;
using namespace captive::devices::timers;
using namespace captive::loader;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;
using namespace captive::platform;
using namespace captive::util;

int main(int argc, char **argv)
{
	const CommandLine *cl = CommandLine::parse(argc, argv);

	if (cl::Help) {
		return 1;
	}
	
	captive::logging::configure_logging_contexts();

	if (argc < 5 || argc > 7) {
		ERROR << "usage: " << argv[0] << " <engine lib> <zimage> <device tree> <root fs>";
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
	TickSource *ts = new MicrosecondTickSource();

	// Create the guest platform.
	Platform *pfm = new Realview(*ts, std::string(argv[4]));

	// Create the engine.
	Engine engine(argv[1]);
	if (!engine.init()) {
		delete pfm;
		delete hv;

		ERROR << "Unable to initialise engine";
		return 1;
	}

	Guest *guest = hv->create_guest(engine, *pfm);
	if (!guest) {
		delete pfm;
		delete hv;

		ERROR << "Unable to create guest VM";
		return 1;
	}

	// Initialise the guest
	if (!guest->init()) {
		delete guest;
		delete pfm;
		delete hv;

		ERROR << "Unable to initialise guest VM";
		return 1;
	}

	// Load the kernel
	auto kernel = KernelLoader::create_from_file(argv[2]);
	if (!kernel) {
		delete guest;
		delete pfm;
		delete hv;

		ERROR << "Unable to detect type of guest kernel";
		return 1;
	}
	
	if (!guest->load(*kernel)) {
		delete guest;
		delete pfm;
		delete hv;

		ERROR << "Unable to load guest kernel";
		return 1;
	}

	guest->guest_entrypoint(kernel->entrypoint());
	
	/*InitRDLoader initrd(argv[3], 0x8000000);
	if (!guest->load(initrd)) {
		delete guest;
		delete pfm;
		delete hv;

		ERROR << "Unable to load initrd";
		return 1;
	}*/

	// Load the device-tree
	if (kernel->requires_device_tree()) {
		/*DeviceTreeLoader device_tree(argv[3], 0x1000);
		if (!guest->load(device_tree)) {
			delete guest;
			delete pfm;
			delete hv;

			ERROR << "Unable to load device tree";
			return 1;
		}*/
		
		// Load atags
		ATAGsLoader atags; //(initrd);
		if (!guest->load(atags)) {
			delete guest;
			delete pfm;
			delete hv;

			ERROR << "Unable to load ATAGs";
			return 1;
		}
	}
		
	// Start the tick source.
	ts->start();

	// Start the platform.
	pfm->start();
	
	if (!guest->run()) {
		ERROR << "Unable to run CPU";
	}

	pfm->stop();
	
	// Close the block device file
	// XXX: TODO: FIXME
	//bdev->close_file();

	// Stop the tick source
	ts->stop();
	delete ts;
	
	// Clean-up
	delete guest;
	delete pfm;
	delete hv;

	DEBUG << CONTEXT(Main) << "Complete";

	return 0;
}
