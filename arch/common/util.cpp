#include <define.h>
#include <printf.h>
#include <malloc.h>

void *operator new(size_t size)
{
	return captive::arch::malloc(size);
}

void *operator new[](size_t size)
{
	return captive::arch::calloc(1, size);
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

namespace std {
	void __throw_out_of_range(const char *message)
	{
		printf("out of range exception: %s\n", message);
		abort();
	}
}
