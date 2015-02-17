#include <devices/arm/sp810.h>

#include <chrono>

using namespace captive::devices::arm;

SP810::SP810() : Primecell(0x00041011), hr_begin()
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
		data = 0; //(std::chrono::duration_cast<tick_24MHz_t>(std::chrono::milliseconds(clock_t::now() - hr_begin))).count();
		break;
	}
	}
	return true;
}

bool SP810::write(uint64_t off, uint8_t len, uint64_t data)
{
	return false;
}
