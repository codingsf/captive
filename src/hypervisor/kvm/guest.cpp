#include <hypervisor/kvm/kvm.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/cpu.h>
#include <hypervisor/config.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

using namespace captive::engine;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

KVMGuest::KVMGuest(KVM& owner, const GuestConfiguration& config, int fd) : Guest(owner, config), fd(fd)
{
	
}

KVMGuest::~KVMGuest()
{
	for (auto cpu : kvm_cpus) {
		delete cpu;
	}
	
	kvm_cpus.clear();
	
	DEBUG << "Closing KVM VM";
	close(fd);
}

bool KVMGuest::init()
{
	for (auto cpu : config().cpus) {
		int cpu_fd = ioctl(fd, KVM_CREATE_VCPU, 0);
		if (cpu_fd < 0) {
			ERROR << "Failed to create KVM VCPU";
			return false;
		}
		
		KVMCpu *kvm_cpu = new KVMCpu(*this, cpu, cpu_fd);
		if (!kvm_cpu->init()) {
			delete kvm_cpu;
			
			ERROR << "Failed to initialise virtual CPU";
			return false;
		}
		
		kvm_cpus.push_back(kvm_cpu);
	}
	
	return true;
}

bool KVMGuest::start(Engine& engine)
{
	DEBUG << "Starting " << config().name;
	return false;
}

