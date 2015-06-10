#include <devices/timers/microsecond-tick-source.h>
#include <captive.h>
#include <thread>
#include <time.h>

using namespace captive::devices::timers;

MicrosecondTickSource::MicrosecondTickSource() : tick_thread(NULL), terminate(false)
{
}

MicrosecondTickSource::~MicrosecondTickSource()
{
	stop();
}

void MicrosecondTickSource::start()
{
	if (tick_thread) return;

	terminate = false;
	tick_thread = new std::thread(tick_thread_proc, this);
}

void MicrosecondTickSource::stop()
{
	if (!tick_thread) return;

	terminate = true;
	if (tick_thread->joinable())
		tick_thread->join();

	delete tick_thread;
	tick_thread = NULL;
}

void MicrosecondTickSource::tick_thread_proc(MicrosecondTickSource *o)
{
	struct timespec rqtp, rmtp;

	rqtp.tv_nsec = 1e3;
	rqtp.tv_sec = 0;

	while (!o->terminate) {
		o->tick(1);

		if (nanosleep(&rqtp, &rmtp)) {
			ERROR << "Interrupted Sleep";
		}
	}
}

void MicrosecondTickSource::recalibrate()
{
	//
}
