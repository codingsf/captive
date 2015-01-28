#include <captive.h>
#include <hypervisor/kvm/kvm.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

#define KVM_DEVICE_LOCATION		"/dev/kvm"

using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

KVM::KVM() : kvm_fd(-1)
{
}

KVM::~KVM()
{
	DEBUG("closing kvm device\n");
	close(kvm_fd);
}

bool KVM::init()
{
	if (!Hypervisor::init())
		return false;
	
	DEBUG("opening kvm device\n");
	kvm_fd = open(KVM_DEVICE_LOCATION, O_RDWR);
	if (kvm_fd < 0)
		return false;
	return true;
}

Guest* KVM::create_guest()
{
	DEBUG("creating KVM VM\n");
	int guest_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);
	if (guest_fd < 0) {
		return NULL;
	}
	
	DEBUG("creating kvm guest object\n");
	return new KVMGuest(*this, guest_fd);
}

int KVM::version() const
{
	return ioctl(kvm_fd, KVM_GET_API_VERSION);	
}

bool KVM::supported()
{
	DEBUG("attempting to access kvm device\n");
	if (access(KVM_DEVICE_LOCATION, F_OK)) {
		return false;
	}

	return true;
}

KVMGuest::KVMGuest(KVM& owner, GuestConfiguration& config, int fd) : Guest(owner, config), fd(fd)
{
	
}

KVMGuest::~KVMGuest()
{
	DEBUG("deleting kvm vm\n");
	close(fd);
}

bool KVMGuest::start()
{
	return false;
}
