#include <define.h>
#include <printf.h>
#include <malloc/malloc.h>

void *operator new(size_t size)
{
	return captive::arch::malloc::data_alloc.alloc(size);
}

void *operator new[](size_t size)
{
	return captive::arch::malloc::data_alloc.calloc(1, size);
}

void operator delete(void *p)
{
	captive::arch::malloc::data_alloc.free(p);
}

extern "C" void __cxa_pure_virtual()
{
	printf("PURE VIRTUAL PAL\n");
	abort();
}

extern "C" {
	void *__dso_handle;
}

extern "C" int __cxa_atexit(void (*destructor) (void *), void *arg, void *dso)
{
	
}

extern "C" void __cxa_finalize(void *f)
{
	
}

namespace std {
	void __throw_out_of_range(const char *message)
	{
		printf("out of range exception: %s\n", message);
		abort();
	}
}
