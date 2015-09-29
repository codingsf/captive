#include <captive.h>
#include <verify.h>
#include <engine/engine.h>

#include <jit/llvm.h>

#include <loader/zimage-loader.h>
#include <loader/elf-loader.h>
#include <loader/devtree-loader.h>
#include <loader/atags-loader.h>

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
using namespace captive::jit;
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
		ERROR << "usage: " << argv[0] << " <engine lib> <zimage> <device tree> <root fs> [{--interp | --block | --region} | --verify {0 | 1}]";
		return 1;
	}

	// Check that KVM is supported
	if (!KVM::supported()) {
		ERROR << "KVM is not supported";
		return 1;
	}

	GuestCPUConfiguration::CPUExecutionMode default_execution_mode = GuestCPUConfiguration::BlockJIT;
	
	if (argc == 7) {
		if (strcmp(argv[5], "--verify")) {
			ERROR << "usage: " << argv[0] << " <engine lib> <zimage> <device tree> <root fs> [--verify {0 | 1}]";
			return 1;
		}

		if (verify_prepare(atoi(argv[6]))) {
			ERROR << "Unable to prepare verification mode";
			return 1;
		}
	} else if (argc == 6) {
		if (strcmp(argv[5], "--region") == 0) {
			default_execution_mode = GuestCPUConfiguration::RegionJIT;
		} else if (strcmp(argv[5], "--block") == 0) {
			default_execution_mode = GuestCPUConfiguration::BlockJIT;
		} else if (strcmp(argv[5], "--interp") == 0) {
			default_execution_mode = GuestCPUConfiguration::Interpreter;
		} else {
			ERROR << "Invalid execution mode";
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
	TickSource *ts;

	if (verify_enabled()) {
		ts = new CallbackTickSource(1000);
	} else {
		ts = new MicrosecondTickSource();
	}

	// Create the guest platform.
	Platform *pfm = new Realview(*ts);

	// Create the engine.
	Engine engine(argv[1]);
	if (!engine.init()) {
		delete pfm;
		delete hv;

		ERROR << "Unable to initialise engine";
		return 1;
	}

	// Create the worker thread pool
	ThreadPool worker_threads("jit-worker-", 0, 1);
	worker_threads.start();

	// Create the JIT
	LLVMJIT jit(engine, worker_threads);
	if (!jit.init()) {
		delete pfm;
		delete hv;

		ERROR << "Unable to initialise jit";
		return 1;
	}

	Guest *guest = hv->create_guest(engine, (JIT&)jit, *pfm);
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
	KernelLoader *kernel = KernelLoader::create_from_file(argv[2]);
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
		
		ATAGsLoader atags;
		if (!guest->load(atags)) {
			delete guest;
			delete pfm;
			delete hv;

			ERROR << "Unable to load ATAGs";
			return 1;
		}
	}
	
	// Load atags

	CPU *cpu = NULL;
	if (verify_enabled()) {
		GuestCPUConfiguration cpu_cfg(verify_get_tid() == 0 ? GuestCPUConfiguration::BlockJIT : GuestCPUConfiguration::BlockJIT, true, (devices::timers::CallbackTickSource *)ts);
		cpu = guest->create_cpu(cpu_cfg);
	} else {
		GuestCPUConfiguration cpu_cfg(default_execution_mode);
		cpu = guest->create_cpu(cpu_cfg);
	}

	if (!cpu) {
		delete guest;
		delete pfm;
		delete hv;

		ERROR << "Unable to create CPU";
		return 1;
	}

	if (!cpu->init()) {
		delete cpu;
		delete guest;
		delete pfm;
		delete hv;

		ERROR << "Unable to initialise CPU";
		return 1;
	}

	// Start the tick source.
	ts->start();

	// Start the platform.
	pfm->start();
	
	// XXX: TODO: FIXME
	//vs->cpu(*cpu);

	if (!cpu->run()) {
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
	delete cpu;
	delete guest;
	delete pfm;
	delete hv;

	DEBUG << CONTEXT(Main) << "Complete";

	return 0;
}
