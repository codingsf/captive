#include <jit/allocator.h>

using namespace captive::jit;

Allocator::Allocator(void* arena, uint64_t arena_size) : _arena(arena), _arena_next((void *)((uint64_t)arena + 0x1000)), _arena_size(arena_size)
{

}

Allocator::~Allocator()
{

}

AllocationRegion* Allocator::allocate(size_t size)
{
	void *data = _arena_next;
	size_t original_size = size;

	if (size % 16) {
		size += 16 - (size % 16);
	}

	_arena_next = (void *)((uint64_t)_arena_next + size);

	return new AllocationRegion(*this, data, original_size, size);
}

void Allocator::free(AllocationRegion& region)
{
	delete &region;
}

bool Allocator::resize(AllocationRegion& region, size_t new_size)
{
	return false;
}
