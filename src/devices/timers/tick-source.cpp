#include <devices/timers/tick-source.h>

using namespace captive::devices::timers;

TickSource::TickSource() : _count(0)
{

}

TickSource::~TickSource()
{

}

void TickSource::tick(uint32_t period)
{
	_count += period;

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
