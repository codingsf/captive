#include <hypervisor/kvm/cpu.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/kvm.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

using namespace captive::hypervisor::kvm;

KVMCpu::KVMCpu(KVMGuest& owner, const GuestCPUConfiguration& config, int id, int fd) : CPU(owner, config), _id(id), fd(fd), cpu_run_struct(NULL)
{

}

KVMCpu::~KVMCpu()
{
	munmap(cpu_run_struct, sizeof(*cpu_run_struct));

	DEBUG << "Closing KVM VCPU fd";
	close(fd);
}

bool KVMCpu::init()
{
	if (!CPU::init())
		return false;

	cpu_run_struct = (struct kvm_run *)mmap(NULL, sizeof(*cpu_run_struct), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (cpu_run_struct == MAP_FAILED) {
		ERROR << "Unable to mmap CPU run struct";
		return false;
	}

	return true;
}

bool KVMCpu::run()
{
	DEBUG << "Running CPU " << id();

	int rc = ioctl(fd, KVM_RUN);
	if (rc != 0) {
		ERROR << "Unable to run VCPU: " << strerror(errno);
		return false;
	}

	return true;
}
