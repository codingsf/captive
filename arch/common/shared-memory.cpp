#include <shared-memory.h>
#include <string.h>
#include <printf.h>

using namespace captive::arch;

SharedMemory::SharedMemory(void* arena, uint64_t arena_size)
	: _arena(arena),
	_arena_size(arena_size),
	_header((struct shared_memory_header *)arena)
{

}

void* SharedMemory::allocate(size_t size, alloc_flags_t flags)
{
	spinlock_wrapper lock(&_header->lock);
	
	struct shared_memory_block **block_slot = &(_header->first);

	if (size % 64) {
		size += 64 - (size % 64);
	}

	while (*block_slot) {
		if ((*block_slot)->size > size) {
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
			break;
		}

		block_slot = &((*block_slot)->next);
	}

	if (!*block_slot) {
		return NULL;
	}

	struct shared_memory_block *found_block = *block_slot;

	*block_slot = (*block_slot)->next;

	return (void *)(&found_block->data[0]);
}

void* SharedMemory::reallocate(void* p, size_t size)
{
	assert(false);
}

void SharedMemory::free(void* p)
{
	assert(false);
}
