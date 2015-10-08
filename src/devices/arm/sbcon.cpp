#include <devices/arm/sbcon.h>

using namespace captive::devices::arm;

SBCON::SBCON()
{
	
}

SBCON::~SBCON()
{
	
}

bool SBCON::read(uint64_t off, uint8_t len, uint64_t& data)
{
	return true;
}

bool SBCON::write(uint64_t off, uint8_t len, uint64_t data)
{
	return true;
}
