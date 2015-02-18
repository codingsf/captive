#include <devices/arm/versatile-sic.h>

using namespace captive::devices::arm;

VersatileSIC::VersatileSIC()
{

}

VersatileSIC::~VersatileSIC()
{

}

bool VersatileSIC::read(uint64_t off, uint8_t len, uint64_t& data)
{
	switch (off) {
	case 0x00:
		data = status & enable_mask;
		break;

	case 0x04:
		data = status;
		break;

	case 0x08:
		data = enable_mask;
		break;
	}

	return true;
}

bool VersatileSIC::write(uint64_t off, uint8_t len, uint64_t data)
{
	switch (off) {
	case 0x08:
		enable_mask |= data;
		break;

	case 0x0c:
		enable_mask &= ~data;
		break;
	}

	return true;
}
