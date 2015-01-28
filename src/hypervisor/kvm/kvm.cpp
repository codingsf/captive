#include <captive.h>
#include <hypervisor/kvm/kvm.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

KVM::KVM()
{
	DEBUG("opening kvm device\n");
	kvm_fd = open("/dev/kvm", O_RDWR);
	if (kvm_fd < 0)
		throw new CaptiveException();
}

KVM::~KVM()
{
	DEBUG("closing kvm device\n");
	close(kvm_fd);
}

Guest* KVM::create_guest()
{
	DEBUG("creating kvm guest object\n");
	return new KVMGuest(*this);
}

bool KVM::supported()
{
	DEBUG("attempting to access kvm device\n");
	if (access("/dev/kvm", F_OK)) {
		return false;
	}

	return true;
}

KVMGuest::KVMGuest(Hypervisor& owner) : Guest(owner)
{

}

KVMGuest::~KVMGuest()
{

}

void KVMGuest::start()
{

}
