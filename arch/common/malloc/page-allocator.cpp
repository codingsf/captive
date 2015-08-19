#include <malloc/page-allocator.h>

using namespace captive::arch::malloc;

PageAllocator::PageAllocator() : _heap(NULL) { }

void PageAllocator::init(void* heap, size_t heap_size)
{
	_heap = heap;
	_heap_size = heap_size;
	_next_page = heap;
}

void *PageAllocator::alloc_pages(int nr_pages)
{
	if (!_heap) return NULL;
	
	void *pages = _next_page;
	_next_page = (void *)((uint64_t)_next_page + (nr_pages * PAGE_SIZE));
	return pages;
}

void PageAllocator::free_pages(void *p)
{
	//
}

namespace captive { namespace arch { namespace malloc {
	PageAllocator page_alloc;
}}}
