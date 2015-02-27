#include <define.h>
#include <printf.h>
#include <malloc.h>

void *operator new(size_t size)
{
	return captive::arch::malloc(size);
}

void operator delete(void *p)
{
	captive::arch::free(p);
}

extern "C" void __cxa_pure_virtual()
{
	printf("PURE VIRTUAL PAL\n");
	abort();
}
