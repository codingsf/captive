#include <mm.h>
#include <printf.h>

#include "include/mm.h"

using namespace captive::arch;

Memory *Memory::mm;

Memory::Memory(uint64_t first_phys_page) : _first_phys_page(first_phys_page), _data_base((void *)0x200000000)
{
	mm = this;
	printf("first: %x, data: %x\n", _first_phys_page, _data_base);
}

void* Memory::alloc_page()
{
	uint64_t phys_page = mm->_first_phys_page;
	mm->_first_phys_page += 0x1000;

	return (void *)&((uint8_t *)mm->_data_base)[phys_page];
}
