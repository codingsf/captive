#include <define.h>
#include <printf.h>

static uint8_t *heap = (uint8_t *)0x110000000;

void *operator new(size_t size)
{
	printf("new: %x\n", heap);

	uint8_t *obj = heap;
	heap += size;

	return (void *)obj;
}

void operator delete(void *p)
{

}

extern "C" void __cxa_pure_virtual()
{
	printf("PURE VIRTUAL PAL\n");
	asm volatile("out %0, $0xff\n" : : "a"(0x02));
}
