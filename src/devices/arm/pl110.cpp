#include <devices/arm/pl110.h>

using namespace captive::devices::arm;

PL110::PL110(gfx::VirtualScreen& screen) : Primecell(0x00041110, 0x10000), _screen(screen)
{

}

PL110::~PL110()
{

}

bool PL110::read(uint64_t off, uint8_t len, uint64_t& data)
{
	if (Primecell::read(off, len, data))
		return true;
	return true;
}

bool PL110::write(uint64_t off, uint8_t len, uint64_t data)
{
	if (Primecell::write(off, len, data))
		return true;
	return true;
}
