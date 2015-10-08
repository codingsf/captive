#include <devices/arm/pl041.h>

using namespace captive::devices::arm;

PL041::PL041() : Primecell(0)
{

}

PL041::~PL041()
{

}

bool PL041::read(uint64_t off, uint8_t len, uint64_t& data)
{
	Primecell::read(off, len, data);
	return true;
}

bool PL041::write(uint64_t off, uint8_t len, uint64_t data)
{
	Primecell::write(off, len, data);
	return true;
}
