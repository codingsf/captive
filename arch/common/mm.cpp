#include <mm.h>
#include <printf.h>

using namespace captive::arch;

Memory::Memory(uint64_t first_phys_page) : _first_phys_page(first_phys_page), _data_base((void *)0x200000000)
{
	printf("first: %x\n", first_phys_page);
}
