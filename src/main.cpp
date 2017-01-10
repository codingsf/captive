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
#include <platform/gensim-test.h>
#include <platform/juno.h>

#include <simulation/insn-count.h>
#include <simulation/cache/cache-simulation.h>

#include <util/command-line.h>
#include <util/config/config.h>

#include <devices/timers/timer-manager.h>

#include <signal.h>
#include <execinfo.h>
#include <malloc.h>

DECLARE_CONTEXT(Main);

using namespace captive;
using namespace captive::engine;
using namespace captive::devices::timers;
using namespace captive::loader;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;
using namespace captive::platform;
using namespace captive::util;

static void handle_segv(int sig, siginfo_t *siginfo, void *ucontext)
{
	void **backtrace_buffer = (void **)calloc(64, sizeof(void *));
	char **backtrace_syms;
	
	fprintf(stderr, "error: seg-fault @ %p\n", siginfo->si_addr);
	
	int nr = backtrace(backtrace_buffer, 64);
	fprintf(stderr, "backtrace: %d frames\n", nr);
	
	backtrace_syms = backtrace_symbols(backtrace_buffer, nr);
	if (backtrace_syms) {
		for (int i = 0; i < nr; i++) {
			fprintf(stderr, "[%02d] %s\n", i, backtrace_syms[i]);
		}
	}
	
	exit(1);
}

static Guest *current_guest;
static void handle_intr(int sig, siginfo_t *siginfo, void *ucontext)
{
	fprintf(stderr, "*** received SIGINT\n");
	if (current_guest) {
		current_guest->stop();
	}
}

static void handle_usr(int sig, siginfo_t *siginfo, void *ucontext)
{
	switch (sig) {
	case SIGUSR1:
		if (current_guest) {
			current_guest->debug_interrupt(0);
		}
		break;
	case SIGUSR2:
		if (current_guest) {
			current_guest->debug_interrupt(1);
		}
		break;
	}
}

static void init_signals()
{
	struct sigaction segv_action;
	segv_action.sa_sigaction = handle_segv;
	segv_action.sa_flags = SA_SIGINFO;

	struct sigaction intr_action;
	intr_action.sa_sigaction = handle_intr;
	intr_action.sa_flags = SA_SIGINFO;

	struct sigaction usr_action;
	usr_action.sa_sigaction = handle_usr;
	usr_action.sa_flags = SA_SIGINFO;
	
	sigaction(SIGSEGV, &segv_action, NULL);
	sigaction(SIGINT, &intr_action, NULL);
	sigaction(SIGUSR1, &usr_action, NULL);
	sigaction(SIGUSR2, &usr_action, NULL);
}

int main(int argc, const char **argv)
{
	init_signals();
	
	util::config::Configuration cfg;
	
	if (!CommandLine::parse(argc, argv, cfg)) {
		CommandLine::print_usage();
		return 1;
	}
	
	if (cfg.print_usage) {
		CommandLine::print_usage();
		return 0;
	}
	
	captive::logging::configure_logging_contexts();

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

	// Create the timer manager
	TimerManager timer_manager;

	// Create the guest platform.
	Platform *pfm;
	
	if (cfg.platform == "juno") {
		pfm = new Juno(cfg, timer_manager, Juno::CORTEX_A72);
	} else if (cfg.platform == "realview") {
		pfm = new Realview(cfg, timer_manager, Realview::CORTEX_A8);
	} else {
		ERROR << "Unrecognised platform: " << cfg.platform;
		return 1;
	}
		
	Engine engine(cfg.arch_module);
	if (!engine.init()) {
		delete pfm;
		delete hv;

		ERROR << "Unable to initialise engine";
		return 1;
	}
		
	auto kernel = KernelLoader::create_from_file(cfg.guest_kernel);
	if (!kernel) {
		delete pfm;
		delete hv;

		ERROR << "Unable to detect type of guest kernel";
		return 1;
	}

	Guest *guest = hv->create_guest(engine, *pfm);
	if (!guest) {
		delete pfm;
		delete hv;

		ERROR << "Unable to create guest VM";
		return 1;
	}
	
	current_guest = guest;
	
	// Initialise the guest
	if (!guest->init()) {
		delete guest;
		delete pfm;
		delete hv;

		ERROR << "Unable to initialise guest VM";
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
	
	if (cfg.cache_simulation) {
		auto sim = new simulation::cache::CacheSimulation();
		guest->add_simulation(*sim);
	}
	
	if (cfg.insn_count) {
		auto sim = new simulation::InstructionCounter();
		guest->add_simulation(*sim);
	}

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
		/*ATAGsLoader atags; //(initrd);
		if (!guest->load(atags)) {
			delete guest;
			delete pfm;
			delete hv;

			ERROR << "Unable to load ATAGs";
			return 1;
		}*/
	}
		
	// Start the timer manager
	timer_manager.start();

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
	timer_manager.stop();
	
	// Clean-up
	delete guest;
	delete pfm;
	delete hv;

	DEBUG << CONTEXT(Main) << "Complete";

	return 0;
}
