#include <platform/platform.h>

using namespace captive;
using namespace captive::platform;
using namespace captive::devices::timers;

Platform::Platform(TimerManager& timer_manager) : _timer_manager(timer_manager)
{
	
}

Platform::~Platform()
{
	
}
