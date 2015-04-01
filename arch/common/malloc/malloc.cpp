#include <malloc.h>
#include <string.h>
#include <printf.h>
#include <shmem.h>

using namespace captive::arch;

static uint8_t *heap, *heap_end, *brk;
static size_t heap_size;

void captive::arch::malloc_init(captive::MemoryVector& arena)
{
	heap = brk = (uint8_t *)arena.base_address;
	heap_size = arena.size;
	heap_end = heap + heap_size;
}

void *sbrk(unsigned long incr)
{
	if (incr == 0)
		return (void *)heap;

	if (brk + incr >= heap_end) {
		return (void *)-1;
	} else {
		brk += incr;
		return (void *)brk;
	}
}
