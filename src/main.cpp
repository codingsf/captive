#include <captive.h>
#include <hypervisor/kvm/kvm.h>

using namespace captive;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

int main(int argc, char **argv)
{
	// Check that KVM is supported
	if (!KVM::supported()) {
		ERROR << "KVM is not supported";
		return 1;
	}
	
	// Create a new hypervisor.
	Hypervisor *hv = new KVM();
	if (!hv->init()) {
		ERROR << "Unable to initialise the hypervisor";
		return 1;
	}
	
	// Attempt to create a guest in the hypervisor.
	GuestConfiguration cfg;
	cfg.name = "test";
	
	Guest *guest = hv->create_guest(cfg);
	if (!guest) {
		delete hv;
		
		ERROR << "Unable to create guest VM";
		return 1;
	}
	
	// Attempt to start the guest.
	if (!guest->start()) {
		delete guest;
		delete hv;

		ERROR << "Unable to start guest VM";
		return 1;
	}
	
	// Clean-up
	delete guest;
	delete hv;
	
	DEBUG << "Complete";
	
	return 0;
}
