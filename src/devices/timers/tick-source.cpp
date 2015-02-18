#include <devices/timers/tick-source.h>

using namespace captive::devices::timers;

TickSource::TickSource()
{

}

TickSource::~TickSource()
{

}

void TickSource::tick(uint32_t period)
{
	for (auto sink : sinks) {
		sink->tick(period);
	}
}

void TickSource::start()
{

}

void TickSource::stop()
{

}
