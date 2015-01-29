#include <captive.h>
#include <hypervisor/config.h>
#include <hypervisor/kvm/kvm.h>
#include <hypervisor/kvm/guest.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

#define KVM_DEVICE_LOCATION		"/dev/kvm"

using namespace captive::engine;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

KVM::KVM() : kvm_fd(-1)
{
}

KVM::~KVM()
{
	DEBUG << "Closing KVM device";
	close(kvm_fd);
	kvm_fd = -1;
}

bool KVM::init()
{
	// Ensure we're not already initialised.
	if (initialised()) {
		ERROR << "KVM Hypervisor already initialised";
		return false;
	}
	
	// Initialise the underlying hypervisor.
	if (!Hypervisor::init())
		return false;
	
	// Attempt to open the KVM device node.
	DEBUG << "Opening KVM device";
	kvm_fd = open(KVM_DEVICE_LOCATION, O_RDWR);
	if (kvm_fd < 0) {
		ERROR << "Unable to open KVM device";
		return false;
	}
	
	return true;
}

Guest* KVM::create_guest(const GuestConfiguration& config)
{
	// Ensure we've been initialised.
	if (!initialised()) {
		ERROR << "KVM Hypervisor not yet initialised";
		return NULL;
	}
	
	// Validate the incoming guest configuration.
	if (!validate_configuration(config)) {
		ERROR << "Invalid configuration";
		return NULL;
	}
	
	// Issue the ioctl to create a new VM.
	DEBUG << "Creating new KVM VM";
	int guest_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);
	if (guest_fd < 0) {
		ERROR << "Failed to create KVM VM";
		return NULL;
	}
	
	// Create (and register) the representative guest object.
	DEBUG << "Creating guest object";
	KVMGuest *guest = new KVMGuest(*this, config, guest_fd);	
	known_guests.push_back(guest);
	
	return guest;
}

bool KVM::validate_configuration(const GuestConfiguration& config) const
{
	// Perform general configuration validation
	if (!config.validate()) {
		return false;
	}
	
	// TODO: KVM specific configuration validation
	return true;
}

int KVM::version() const
{
	if (!initialised()) {
		ERROR << "KVM Hypervisor not yet initialised";
		return -1;
	}
	
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
