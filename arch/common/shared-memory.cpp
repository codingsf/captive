#include <shared-memory.h>

using namespace captive::arch;

SharedMemory::SharedMemory(void* arena, uint64_t arena_size)
	: _arena(arena),
	_arena_size(arena_size),
	_header((struct shared_memory_header *)arena)
{

}

void* SharedMemory::allocate(size_t size)
{
	if (size % 16) {
		size += 16 - (size % 16);
	}

	spin_lock(&_header->lock);

	void *p = _header->next_free;
	_header->next_free = (void *)((uint64_t)_header->next_free + size);

	spin_unlock(&_header->lock);

	return p;
}

void SharedMemory::free(void* p)
{
	//
}
