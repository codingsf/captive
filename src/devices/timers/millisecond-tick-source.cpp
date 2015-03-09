#include <devices/timers/millisecond-tick-source.h>
#include <thread>
#include <unistd.h>

using namespace captive::devices::timers;

MillisecondTickSource::MillisecondTickSource() : tick_thread(NULL), terminate(false)
{
}

MillisecondTickSource::~MillisecondTickSource()
{
	stop();
}

void MillisecondTickSource::start()
{
	if (tick_thread) return;

	terminate = false;
	tick_thread = new std::thread(tick_thread_proc, this);
}

void MillisecondTickSource::stop()
{
	if (!tick_thread) return;

	terminate = true;
	if (tick_thread->joinable())
		tick_thread->join();
	delete tick_thread;
}

void MillisecondTickSource::tick_thread_proc(MillisecondTickSource *o)
{
	while (!o->terminate) {
		o->tick(1);
		usleep(1000);
	}
}
