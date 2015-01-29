#include <hypervisor/kvm/cpu.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/kvm.h>
#include <unistd.h>

using namespace captive::hypervisor::kvm;

KVMCpu::KVMCpu(KVMGuest& owner, const GuestCPUConfiguration& config, int fd) : CPU(owner, config), fd(fd)
{

}

KVMCpu::~KVMCpu()
{
	DEBUG << "Closing KVM VCPU fd";
	close(fd);
}

bool KVMCpu::init()
{
	return CPU::init();
}

void KVMCpu::run()
{
	
}