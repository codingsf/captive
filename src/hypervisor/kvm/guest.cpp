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

KVMGuest::KVMGuest(KVM& owner, Engine& engine, const GuestConfiguration& config, int fd) : Guest(owner, engine, config), fd(fd), next_cpu_id(0)
{

}

KVMGuest::~KVMGuest()
{
	DEBUG << "Closing KVM VM";
	close(fd);
}

bool KVMGuest::init()
{
	return true;
}

CPU* KVMGuest::create_cpu(const GuestCPUConfiguration& config)
{
	if (!config.validate()) {
		ERROR << "Invalid CPU configuration";
		return NULL;
	}

	DEBUG << "Creating KVM VCPU";
	int cpu_fd = ioctl(fd, KVM_CREATE_VCPU, next_cpu_id);
	if (cpu_fd < 0) {
		ERROR << "Failed to create KVM VCPU";
		return NULL;
	}

	KVMCpu *cpu = new KVMCpu(*this, config, next_cpu_id++, cpu_fd);
	kvm_cpus.push_back(cpu);

	return cpu;
}
