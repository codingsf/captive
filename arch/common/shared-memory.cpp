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
	size += sizeof(shared_memory_block);

	if (size % 16) {
		size += 16 - (size % 16);
	}

	spin_lock(&_header->lock);

	void *p = _header->next_free;
	_header->next_free = (void *)((uint64_t)_header->next_free + size);

	spin_unlock(&_header->lock);

	if (flags & ZERO) {
		bzero(p, size);
	}

	struct shared_memory_block *block = (struct shared_memory_block *)p;
	block->block_size = size;

	return (void *)((uint64_t)p + sizeof(struct shared_memory_block));
}

void* SharedMemory::reallocate(void* p, size_t size)
{
	printf("realloc %p %d\n", p, size);

	struct shared_memory_block *block = get_block(p);
	if (block && (size <= (block->block_size - sizeof(struct shared_memory_block)))) {
		return p;
	}

	void *new_p = allocate(size);
	if (!new_p) {
		return NULL;
	}

	if (block) {
		memcpy(new_p, p, block->block_size - sizeof(struct shared_memory_block));
		free(p);
	}

	return new_p;
}

void SharedMemory::free(void* p)
{
	struct shared_memory_block *block = get_block(p);
	if (!block) return;
}
