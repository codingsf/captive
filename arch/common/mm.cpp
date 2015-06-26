#include <mm.h>
#include <printf.h>
#include <shared-memory.h>

using namespace captive::arch;

Memory *Memory::mm;

uint64_t Memory::__force_order;

Memory::Memory(uint64_t first_phys_page, MemoryVector shared_memory_arena)
	: _next_phys_page((pa_t)first_phys_page),
	_data_base((va_t)0x200000000),
	_shared_memory(new SharedMemory())
{
	mm = this;
	//printf("next avail phys page: %x, data area base: %x\n", _next_phys_page, _data_base);
}

Memory::Page Memory::alloc_page()
{
	// Grab one page from the list
	pa_t phys_page = mm->_next_phys_page;
	mm->_next_phys_page = (pa_t)((uint64_t)mm->_next_phys_page + 0x1000);

	// Create a page structure to represent this new page.
	Memory::Page page;
	page.pa = phys_page;
	page.va = (va_t)&((uint8_t *)mm->_data_base)[(uint64_t)phys_page];

	// TODO: Possibly zero out the page
	// bzero(page.va, 0x1000);

	//printf("mm: alloc page pa=%x va=%x\n", page.pa, page.va);
	return page;
}

void Memory::free_page(Page& page)
{
	//printf("mm: free page pa=%x va=%x\n", page.pa, page.va);
}

void Memory::map_page(va_t va, Page& page)
{

}
