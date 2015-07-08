#include <devices/timers/millisecond-tick-source.h>
#include <captive.h>
#include <thread>
#include <time.h>
#include <pthread.h>

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
	tick_thread = NULL;
}

void MillisecondTickSource::tick_thread_proc(MillisecondTickSource *o)
{
	struct timespec rqtp, rmtp;
	pthread_setname_np(pthread_self(), "millisecond-tick");
	
	rqtp.tv_nsec = 1e6;
	rqtp.tv_sec = 0;

	while (!o->terminate) {
		o->tick(1);

		nanosleep(&rqtp, &rmtp);
	}
}

void MillisecondTickSource::recalibrate()
{
	//
}
