#include <hypervisor/kvm/guest.h>

using namespace captive::hypervisor::kvm;

void *KVMGuest::KVMSharedMemory::allocate(size_t size)
{
	return NULL;
}

void KVMGuest::KVMSharedMemory::free(void* p)
{
	//
}
