#include <define.h>
#include <printf.h>

static uint8_t heap[0x10000000];
static uint8_t *heap_ptr = &heap[0];

void *operator new(size_t size)
{
	if (size % 16) {
		size += 16 - (size % 16);
	}

	uint8_t *obj = heap_ptr;
	heap_ptr += size;

	return (void *)obj;
}

void operator delete(void *p)
{

}

extern "C" void __cxa_pure_virtual()
{
	printf("PURE VIRTUAL PAL\n");
	abort();
}
