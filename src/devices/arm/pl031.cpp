#include <devices/arm/pl031.h>
#include <captive.h>
#include <chrono>

using namespace captive::devices::arm;

PL031::PL031() : Primecell(0x00041031), dr(0), match(0), load(0), ctrl(0), mask(0), isr(0)
{
	dr = (uint32_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

PL031::~PL031()
{

}

bool PL031::read(uint64_t off, uint8_t len, uint64_t& data)
{
	if (Primecell::read(off, len, data))
		return true;
	
	switch (off) {
	case 0x000:
		data = dr;
		return true;
		
	case 0x004:
		data = match;
		return true;
		
	case 0x008:
		data = load;
		return true;
		
	case 0x00c:
		data = ctrl;
		return true;
		
	case 0x010:
		data = mask;
		return true;
		
	case 0x014:
		data = isr;
		return true;
		
	case 0x018:
		data = isr & mask;
		return true;
	}
	
	return false;
}

bool PL031::write(uint64_t off, uint8_t len, uint64_t data)
{
	if (!Primecell::write(off, len, data))
		return true;
	
	switch (off) {
	case 0x004:
		match = data;
		return true;
		
	case 0x008:
		load = data;
		return true;
		
	case 0x00c:
		ctrl = data & 1;
		return true;
		
	case 0x010:
		mask = data & 1;
		return true;
		
	case 0x01c:
		isr &= ~data;
		return true;
	}
	
	return false;
}
