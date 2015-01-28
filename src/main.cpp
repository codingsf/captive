#include <captive.h>
#include <hypervisor/soft/soft.h>
#include <hypervisor/kvm/kvm.h>

using namespace captive::hypervisor::kvm;

int main(int argc, char **argv)
{
	if (!KVM::supported()) {
		fprintf(stderr, "kvm not supported\n");
		return 1;
	}
	
	return 0;
}
