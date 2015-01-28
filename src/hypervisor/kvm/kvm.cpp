#include <captive.h>
#include <hypervisor/kvm/kvm.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

KVM::KVM()
{
	kvm_fd = open("/dev/kvm", O_RDWR);
	if (kvm_fd < 0)
		throw new CaptiveException();
}

KVM::~KVM()
{
	close(kvm_fd);
}

void KVM::run_guest(Guest& guest)
{

}

bool KVM::supported()
{
	if (access("/dev/kvm", F_OK)) {
		return false;
	}
	
	return true;
}
