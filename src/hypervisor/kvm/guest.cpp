#include <hypervisor/kvm/kvm.h>
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
		KVMCpu *kvm_cpu = new KVMCpu(*this, cpu);
		if (!kvm_cpu->init()) {
			return false;
		}
		
		kvm_cpus.push_back(kvm_cpu);
	}
	
	return false;
}

bool KVMGuest::start(Engine& engine)
{
	DEBUG << "Starting " << config().name;
	return false;
}

