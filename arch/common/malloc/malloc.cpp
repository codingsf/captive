#include <malloc.h>
#include <string.h>
#include <printf.h>
#include <shmem.h>

using namespace captive::arch;

static uint8_t *heap, *brk;
static size_t heap_size;

void captive::arch::malloc_init(captive::MemoryVector& arena)
{
	heap = brk = (uint8_t *)arena.base_address;
	heap_size = arena.size;
}

extern "C" void *sbrk(int64_t incr)
{
	if (incr == 0)
		return (void *)brk;

	brk += incr;
	return (void *)brk;
}
