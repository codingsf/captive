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
	uint64_t addr;
	asm volatile ("out %2, $0xff" : "=a"(addr) : "D"(size), "a"(10));
	return (void *)addr;
}

void* SharedMemory::reallocate(void* p, size_t size)
{
	assert(false);
}

void SharedMemory::free(void* p)
{
	asm volatile ("out %1, $0xff" : : "D"(p), "a"(11));
}
