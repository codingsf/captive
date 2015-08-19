#include <malloc/code-memory-allocator.h>
#include <malloc/data-memory-allocator.h>

using namespace captive::arch::malloc;

namespace captive { namespace arch { namespace malloc {
CodeMemoryAllocator code_alloc;
extern DataMemoryAllocator data_alloc;
}}}

void *CodeMemoryAllocator::alloc(size_t size)
{
	return data_alloc.alloc(size);
}

void *CodeMemoryAllocator::realloc(void *p, size_t new_size)
{
	return data_alloc.realloc(p, new_size);
}

void CodeMemoryAllocator::free(void *p)
{
	return data_alloc.free(p);
}
