#include <devices/arm/realview/sysctl.h>
#include <devices/timers/tick-source.h>

using namespace captive::devices::arm::realview;

SysCtl::SysCtl(timers::TickSource& tick_source) : _tick_source(tick_source), _start_time(tick_source.count())
{
	
}

bool SysCtl::read(uint64_t off, uint8_t len, uint64_t& data)
{
	switch (off) {
	case 0x000:
		data = 0x01780500;
		return true;
		
	case 0x004:
		data = 0;
		return true;
		
	case 0x008:
		data = 0;
		return true;
		
	case 0x05c:
		data = std::chrono::duration_cast<tick_24MHz_t>(std::chrono::milliseconds(_tick_source.count() - _start_time)).count();
		return true;
	}
	
	fprintf(stderr, "sysctl: unkown register read @ %03x\n", off);
	return false;
}

bool SysCtl::write(uint64_t off, uint8_t len, uint64_t data)
{
	return false;
}
