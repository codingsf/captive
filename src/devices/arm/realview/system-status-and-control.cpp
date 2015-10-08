#include <devices/arm/realview/system-status-and-control.h>
#include <devices/timers/tick-source.h>

#include <captive.h>

DECLARE_CONTEXT(SystemStatusAndControl);

#define LOCK_VALUE 0xa05f

using namespace captive::devices::arm::realview;

SystemStatusAndControl::SystemStatusAndControl(timers::TickSource& tick_source) : 
	_tick_source(tick_source),
	_start_time(tick_source.count()),
	osc { 0x00012c5c, 0x00002cc0, 0x00002c75, 0x00020211, 0x00002c75 },
	colour_mode(0x1f00),
	lockval(0),
	leds(0)
{
	
}

bool SystemStatusAndControl::read(uint64_t off, uint8_t len, uint64_t& data)
{
	switch (off) {
	case 0x000:
		data = 0x01780500;
		return true;
		
	case 0x004:
		data = 0;
		return true;
		
	case 0x008:
		data = leds;
		return true;
		
	case 0x020:
		data = lockval;
		return true;
		
	case 0x05c:
		data = std::chrono::duration_cast<tick_24MHz_t>(std::chrono::milliseconds(_tick_source.count() - _start_time)).count();
		return true;
		
	case 0x50:
		data = colour_mode;
		return true;
		
	case 0x00c ... 0x001c:
		data = osc[(off - 0xc) >> 2];
		return true;
	}
	
	ERROR << CONTEXT(SystemStatusAndControl) << "Unknown register read @ " << std::hex << off;
	return false;
}

bool SystemStatusAndControl::write(uint64_t off, uint8_t len, uint64_t data)
{
	switch (off) {
	case 0x008:
		leds = data;
		return true;
		
	case 0x00c ... 0x01c:
		osc[(off - 0x00c) >> 2] = data;
		return true;

	case 0x20:
		if (data == LOCK_VALUE) lockval = data;
		else lockval = data & 0x7fff;
		return true;
		
	case 0x050:
		colour_mode &= 0x3f00;
		colour_mode |= (data & ~0x3f00);
		return true;
	}
	
	ERROR << CONTEXT(SystemStatusAndControl) << "Unknown register write @ " << std::hex << off << " = " << data;
	return false;
}
