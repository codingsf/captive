#include <devices/arm/pl081.h>

using namespace captive::devices::arm;

PL081::PL081() : Primecell(0) //0x00051081)
{

}

PL081::~PL081()
{

}

bool PL081::read(uint64_t off, uint8_t len, uint64_t& data)
{
	Primecell::read(off, len, data);
	return true;
}

bool PL081::write(uint64_t off, uint8_t len, uint64_t data)
{
	Primecell::write(off, len, data);
	return true;
}
