#include <malloc.h>
#include <string.h>

using namespace captive::arch;

static uint8_t heap[0x10000000];
static uint8_t *heap_ptr = &heap[0];

void *captive::arch::malloc(size_t size)
{
	if (size % 16) {
		size += 16 - (size % 16);
	}

	uint8_t *obj = heap_ptr;
	heap_ptr += size;

	return (void *)obj;
}

void *captive::arch::realloc(void *p, size_t size)
{
	void *nm = malloc(size);
	memcpy(nm, p, size);
	free(p);

	return nm;
}

void captive::arch::free(void *p)
{

}
