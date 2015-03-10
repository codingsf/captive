#include <malloc.h>
#include <string.h>
#include <printf.h>

using namespace captive::arch;

static uint8_t *heap;
static size_t heap_size;

static uint8_t *heap_free_ptr;
static uint8_t *heap_end_ptr;

void captive::arch::malloc_init(void *arena, size_t size)
{
	printf("malloc: arena=%p, size=0x%x\n", arena, size);
	heap = (uint8_t *)arena;
	heap_size = size;

	heap_free_ptr = heap;
	heap_end_ptr = heap + heap_size;
}

void *captive::arch::malloc(size_t size)
{
	size += sizeof(struct malloc_unit);
	if (size % 16) {
		size += 16 - (size % 16);
	}

	if (heap_free_ptr > (heap_end_ptr - size)) {
		return NULL;
	}

	struct malloc_unit *unit = (struct malloc_unit *)heap_free_ptr;
	heap_free_ptr += size;

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
