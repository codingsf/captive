#include <captive.h>
#include <hypervisor/kvm/kvm.h>

using namespace captive;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

int main(int argc, char **argv)
{
	// Check that KVM is supported
	if (!KVM::supported()) {
		fprintf(stderr, "kvm not supported\n");
		return 1;
	}
	
	// Create a new hypervisor.
	Hypervisor *hv = new KVM();
	if (!hv->init()) {
		fprintf(stderr, "unable to initialise hypervisor\n");
		return 1;
	}
	
	// Attempt to create a guest in the hypervisor.
	Guest *guest = hv->create_guest();
	if (!guest) {
		delete hv;
		
		fprintf(stderr, "unable to create vm\n");
		return 1;
	}
	
	// Attempt to start the guest.
	if (!guest->start()) {
		delete guest;
		delete hv;

		fprintf(stderr, "unable to start vm\n");
		return 1;
	}
	
	// Clean-up
	delete guest;
	delete hv;
	
	return 0;
}
