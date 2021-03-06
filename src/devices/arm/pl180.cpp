#include <devices/arm/pl180.h>

using namespace captive::devices::arm;

PL180::PL180() : Primecell(0x00041180)
{

}

PL180::~PL180()
{

}

bool PL180::read(uint64_t off, uint8_t len, uint64_t& data)
{
	Primecell::read(off, len, data);
	return true;
}

bool PL180::write(uint64_t off, uint8_t len, uint64_t data)
{
	Primecell::write(off, len, data);
	return true;
}
