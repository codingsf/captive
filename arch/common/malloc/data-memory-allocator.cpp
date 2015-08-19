#include <malloc/data-memory-allocator.h>

extern "C" {
	void *dlmalloc(size_t);
	void *dlrealloc(void *, size_t);
	void dlfree(void *);
}

using namespace captive::arch::malloc;

void *DataMemoryAllocator::alloc(size_t size)
{
	return dlmalloc(size);
}

void *DataMemoryAllocator::realloc(void *p, size_t new_size)
{
	return dlrealloc(p, new_size);
}

void DataMemoryAllocator::free(void *p)
{
	dlfree(p);
}

namespace captive { namespace arch { namespace malloc {
DataMemoryAllocator data_alloc;
}}}
