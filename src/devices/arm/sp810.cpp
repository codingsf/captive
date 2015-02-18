#include <devices/arm/sp810.h>

#include <chrono>

#define LOCK_VALUE 0xa05f

using namespace captive::devices::arm;

SP810::SP810() : Primecell(0x00041011), hr_begin(clock_t::now()), leds(0), lockval(0), colour_mode(0x1f00)
{

}

SP810::~SP810()
{

}

bool SP810::read(uint64_t off, uint8_t len, uint64_t& data)
{
	switch (off) {
	case 0x5c:
	{
		auto diff = (clock_t::now() - hr_begin);
		data = std::chrono::duration_cast<tick_24MHz_t>(diff).count();
		break;
	}
	}
	return true;
}

bool SP810::write(uint64_t off, uint8_t len, uint64_t data)
{
	switch (off) {
	case 0x08:	// LEDs
		leds = data;
		break;
	case 0x20:	// Lock Val
		if (data == LOCK_VALUE) lockval = data;
		else lockval = data & 0x7fff;
		break;
	case 0x50:	// LCD
		colour_mode &= 0x3f00;
		colour_mode |= (data & ~0x3f00);
		break;
	}
	return true;
}
