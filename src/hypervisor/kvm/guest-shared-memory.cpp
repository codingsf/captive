#include <hypervisor/kvm/guest.h>

using namespace captive::hypervisor::kvm;

KVMGuest::KVMSharedMemory::KVMSharedMemory() : _arena(NULL), _arena_size(0)
{

}


void *KVMGuest::KVMSharedMemory::allocate(size_t size)
{
	size += sizeof(struct shared_memory_block);

	if (size % 16) {
		size += 16 - (size % 16);
	}

	spin_lock(&_header->lock);

	void *p = _header->next_free;
	_header->next_free = (void *)((uint64_t)_header->next_free + size);

	spin_unlock(&_header->lock);

	struct shared_memory_block *block = (struct shared_memory_block *)p;
	block->block_size = size;

	return (void *)((uint64_t)p + sizeof(struct shared_memory_block));
}

void KVMGuest::KVMSharedMemory::free(void* p)
{
	//
}
