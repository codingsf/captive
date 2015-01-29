#include <hypervisor/kvm/kvm.h>
#include <unistd.h>

using namespace captive::engine;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

KVMGuest::KVMGuest(KVM& owner, const GuestConfiguration& config, int fd) : Guest(owner, config), fd(fd)
{
	
}

KVMGuest::~KVMGuest()
{
	DEBUG << "Closing KVM VM";
	close(fd);
}

bool KVMGuest::start(Engine& engine)
{
	DEBUG << "Starting " << config().name;
	return false;
}

