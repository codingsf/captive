#include <devices/arm/pl031.h>
#include <captive.h>

using namespace captive::devices::arm;

PL031::PL031() : Primecell(0x00041031, 0x1000)
{

}

PL031::~PL031()
{

}

bool PL031::read(uint64_t off, uint8_t len, uint64_t& data)
{
	if (Primecell::read(off, len, data))
		return true;
	
	DEBUG << "FOO";
	
	return true;
}

bool PL031::write(uint64_t off, uint8_t len, uint64_t data)
{
	if (Primecell::write(off, len, data))
		return true;
	
	DEBUG << "BAR";
	return true;
}
