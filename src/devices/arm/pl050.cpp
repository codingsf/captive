#include <devices/arm/pl050.h>

using namespace captive::devices::arm;

PL050::PL050(io::PS2Device& ps2) : Primecell(0x00041050), _ps2(ps2)
{

}

PL050::~PL050()
{

}

bool PL050::read(uint64_t off, uint8_t len, uint64_t& data)
{
	if (Primecell::read(off, len, data))
		return true;
	return true;
}

bool PL050::write(uint64_t off, uint8_t len, uint64_t data)
{
	if (Primecell::write(off, len, data))
		return true;
	return true;

}
