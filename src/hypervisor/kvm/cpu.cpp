#include <hypervisor/kvm/cpu.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/kvm.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

using namespace captive::hypervisor::kvm;

KVMCpu::KVMCpu(KVMGuest& owner, const GuestCPUConfiguration& config, int id, int fd) : CPU(owner, config), _initialised(false), _id(id), fd(fd), cpu_run_struct(NULL), cpu_run_struct_size(0)
{

}

KVMCpu::~KVMCpu()
{
	if (initialised()) {
		munmap(cpu_run_struct, cpu_run_struct_size);
	}

	DEBUG << "Closing KVM VCPU fd";
	close(fd);
}

bool KVMCpu::init()
{
	if (initialised()) {
		ERROR << "KVM CPU already initialised";
		return false;
	}

	if (!CPU::init())
		return false;

	cpu_run_struct_size = ioctl(((KVM &)((KVMGuest &)owner()).owner()).kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if ((int)cpu_run_struct_size == -1) {
		ERROR << "Unable to ascertain size of CPU run structure";
		return false;
	}

	DEBUG << "Mapping CPU run structure size=" << std::hex << cpu_run_struct_size;
	cpu_run_struct = (struct kvm_run *)mmap(NULL, cpu_run_struct_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (cpu_run_struct == MAP_FAILED) {
		ERROR << "Unable to mmap CPU run struct";
		return false;
	}

	_initialised =true;
	return true;
}

bool KVMCpu::run()
{
	if (!initialised()) {
		ERROR << "CPU not initialised";
		return false;
	}

	DEBUG << "Running CPU " << id();

	int rc = ioctl(fd, KVM_RUN, 0);
	if (rc != 0) {
		ERROR << "Unable to run VCPU: " << strerror(errno);
		return false;
	}

	return true;
}
