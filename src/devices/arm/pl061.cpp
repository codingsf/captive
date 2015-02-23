#include <devices/arm/pl061.h>

using namespace captive::devices::arm;

PL061::PL061(irq::IRQLine& irq) : Primecell(0x1000), _irq(irq)
{

}

PL061::~PL061()
{

}

bool PL061::read(uint64_t off, uint8_t len, uint64_t& data)
{
	if (Primecell::read(off, len, data))
		return true;
	return true;
}

bool PL061::write(uint64_t off, uint8_t len, uint64_t data)
{
	if (Primecell::write(off, len, data))
		return true;
	return true;
}
