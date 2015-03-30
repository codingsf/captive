#include <hypervisor/kvm/guest.h>
#include <util/spin-lock.h>
#include <captive.h>

DECLARE_CONTEXT(SharedMemoryAllocator);

using namespace captive::hypervisor::kvm;

KVMGuest::KVMSharedMemory::KVMSharedMemory() : _arena(NULL), _arena_size(0)
{

}


void *KVMGuest::KVMSharedMemory::allocate(size_t size)
{
	captive::util::SpinLockWrapper lock(&_header->lock);
	
	struct shared_memory_block **block_slot = &(_header->first);

	if (size % 64) {
		size += 64 - (size % 64);
	}

	DEBUG << "Looking for block at least " << std::hex << size;
	while (*block_slot) {
		DEBUG << "Considering block @ 0x" << std::hex << (uint64_t)(*block_slot) << ", size=" << (*block_slot)->size;

		if ((*block_slot)->size > size) {
			DEBUG << "Splitting block";

			struct shared_memory_block *this_block = (*block_slot);
			uint64_t old_size = this_block->size;
			this_block->size = size;

			struct shared_memory_block *next_block = this_block->next;
			struct shared_memory_block *new_block = (struct shared_memory_block *)((uint64_t)this_block + this_block->size + 8);

			this_block->next = new_block;
			new_block->next = next_block;
			new_block->size = old_size - size;

			break;
		} else if ((*block_slot)->size == size) {
			DEBUG << "Exact match!";
			break;
		}

		DEBUG << "Moving on...";
		block_slot = &((*block_slot)->next);
	}

	if (!*block_slot) {
		WARNING << "Out of Memory";
		return NULL;
	}

	struct shared_memory_block *found_block = *block_slot;

	DEBUG << "Removing from list";
	*block_slot = (*block_slot)->next;

	return (void *)(&found_block->data[0]);
}

void KVMGuest::KVMSharedMemory::free(void* p)
{
	//
}

void KVMGuest::KVMSharedMemory::initialise()
{
	struct shared_memory_block *block = (struct shared_memory_block *)((uint64_t)_header + 64);
	_header->first = block;

	block->next = NULL;
	block->size = (uint64_t)_arena_size - 64 - 8;

	DEBUG << "Initialised shared memory";
}
