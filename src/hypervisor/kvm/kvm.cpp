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
	DEBUG << "Closing KVM device";
	close(kvm_fd);
}

bool KVM::init()
{
	if (kvm_fd != -1)
		return false;
	
	if (!Hypervisor::init())
		return false;
	
	DEBUG << "Opening KVM device";
	kvm_fd = open(KVM_DEVICE_LOCATION, O_RDWR);
	if (kvm_fd < 0)
		return false;
	return true;
}

Guest* KVM::create_guest(const GuestConfiguration& config)
{
	DEBUG << "Creating new KVM VM";
	int guest_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);
	if (guest_fd < 0) {
		return NULL;
	}
	
	DEBUG << "Creating guest object";
	return new KVMGuest(*this, config, guest_fd);
}

int KVM::version() const
{
	return ioctl(kvm_fd, KVM_GET_API_VERSION);	
}

bool KVM::supported()
{
	DEBUG << "Attempting to access KVM device";
	if (access(KVM_DEVICE_LOCATION, F_OK)) {
		return false;
	}

	return true;
}

KVMGuest::KVMGuest(KVM& owner, const GuestConfiguration& config, int fd) : Guest(owner, config), fd(fd)
{
	
}

KVMGuest::~KVMGuest()
{
	DEBUG << "Closing KVM VM";
	close(fd);
}

bool KVMGuest::start()
{
	DEBUG << "Starting " << config().name;
	return false;
}
