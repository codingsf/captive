#include <captive.h>
#include <verify.h>
#include <engine/engine.h>

#include <loader/zimage-loader.h>
#include <loader/elf-loader.h>
#include <loader/devtree-loader.h>
#include <loader/atags-loader.h>
#include <loader/initrd-loader.h>

#include <jit/jit.h>

#include <hypervisor/config.h>
#include <hypervisor/cpu.h>
#include <hypervisor/kvm/kvm.h>

#include <platform/realview.h>

#include <util/command-line.h>
#include <util/thread-pool.h>
#include <util/cl/options.h>

#include <devices/timers/callback-tick-source.h>
#include <devices/timers/microsecond-tick-source.h>

#include <signal.h>

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

static void handle_segv(int sig, siginfo_t *siginfo, void *ucontext)
{
	fprintf(stderr, "error: seg-fault @ %p\n", siginfo->si_addr);
	exit(1);
}

int main(int argc, char **argv)
{
	struct sigaction segv_action;
	segv_action.sa_sigaction = handle_segv;
	segv_action.sa_flags = SA_SIGINFO;
	
	sigaction(SIGSEGV, &segv_action, NULL);
	
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
	Platform *pfm = new Realview(*ts, std::string(argv[4]));

	// Create the engine.
	Engine engine(argv[1]);
	if (!engine.init()) {
		delete pfm;
		delete hv;

		ERROR << "Unable to initialise engine";
		return 1;
	}
	
	captive::jit::NullJIT nj;
	Guest *guest = hv->create_guest(engine, nj, *pfm);
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
	
	pfm->add_core(*cpu);

	// Start the tick source.
	ts->start();

	// Start the platform.
	pfm->start();
	
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
