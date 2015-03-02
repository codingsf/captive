#include <malloc.h>
#include <string.h>

using namespace captive::arch;

static uint8_t heap[0x10000000];
static uint8_t *heap_ptr = &heap[0];

void *captive::arch::malloc(size_t size)
{
	/*size += 16 + sizeof(struct malloc_unit);
	size &= 0xf;*/

	size += sizeof(struct malloc_unit);
	if (size % 16) {
		size += 16 - (size % 16);
	}

	struct malloc_unit *unit = (struct malloc_unit *)heap_ptr;
	heap_ptr += size;

	unit->size = size;

	return (void *)((uint8_t *)unit + sizeof(struct malloc_unit));
}

void *captive::arch::realloc(void *p, size_t size)
{
	if (p == NULL) {
		return malloc(size);
	}

	struct malloc_unit *unit = (struct malloc_unit *)((uint8_t *)p - sizeof(struct malloc_unit));

	if (size == unit->size) {
		return p;
	}

	void *newmem = malloc(size);
	if (!newmem) {
		return NULL;
	}

	if (size < unit->size) {
		memcpy(newmem, p, size);
	} else {
		memcpy(newmem, p, unit->size);
	}

	free(p);

	return newmem;
}

void captive::arch::free(void *p)
{

}
