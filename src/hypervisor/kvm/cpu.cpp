#include <hypervisor/kvm/cpu.h>
#include <hypervisor/kvm/kvm.h>

using namespace captive::hypervisor::kvm;

KVMCpu::KVMCpu(KVMGuest& owner, const GuestCPUConfiguration& config) : CPU(owner, config)
{

}

KVMCpu::~KVMCpu()
{

}

bool KVMCpu::init()
{
	return CPU::init();
}

void KVMCpu::run()
{
	
}