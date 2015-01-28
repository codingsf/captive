#include <captive.h>
#include <hypervisor/kvm/kvm.h>

using namespace captive;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

int main(int argc, char **argv)
{
	if (!KVM::supported()) {
		fprintf(stderr, "kvm not supported\n");
		return 1;
	}
	
	try {
		Hypervisor *vm = new KVM();
		Guest *guest = vm->create_guest();
		guest->start();
		
		delete guest;
		delete vm;
	} catch (CaptiveException& e) {
		fprintf(stderr, "unhandled exception\n");
		return 1;
	}

	return 0;
}
