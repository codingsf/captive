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

void *sbrk(unsigned long incr)
{
	printf("sbrk: %p %lu\n", brk, incr);
	if (incr == 0)
		return (void *)heap;

	brk += incr;
	return (void *)brk;
}
