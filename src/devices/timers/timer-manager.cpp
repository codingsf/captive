#include <devices/timers/timer-manager.h>
#include <time.h>
#include <pthread.h>

using namespace captive::devices::timers;

TimerManager::TimerManager() : timer_thread(NULL)
{
	
}

TimerManager::~TimerManager()
{
	stop();
}

void TimerManager::start()
{
	if (timer_thread) return;
	
	start_time = std::chrono::high_resolution_clock::now();
	
	terminate = false;
	timer_thread = new std::thread(timer_thread_proc_tramp, this);
}

void TimerManager::stop()
{
	if (!timer_thread) return;
	
	terminate = true;
	if (timer_thread->joinable()) timer_thread->join();
	timer_thread = NULL;
}

void TimerManager::add_timer(std::chrono::nanoseconds interval, TimerSink& sink)
{
	for (int i = 0; i < MAX_TIMERS; i++) {
		if (!timers[i].valid) {
			timers[i].interval = interval;
			timers[i].remaining = interval;
			timers[i].sink = &sink;
			timers[i].valid = true;
			
			break;
		}
	}
}

void TimerManager::remove_timer(TimerSink& sink)
{

}

void TimerManager::timer_thread_proc_tramp(void* o)
{
	pthread_setname_np(pthread_self(), "timer-mgr");
	((TimerManager *)o)->timer_thread_proc();
}


void TimerManager::timer_thread_proc()
{
	struct timespec ts_in;
	ts_in.tv_sec = 0;
//	ts_in.tv_nsec = 1000000;
	
	std::chrono::high_resolution_clock::time_point before = std::chrono::high_resolution_clock::now();
	std::chrono::high_resolution_clock::time_point after = before;
	while (!terminate) {
		// Calculate the sleep time, with a correction factor for the length of time it took to perform the previous
		// iteration.  Then, sleeeep.
		ts_in.tv_nsec = 1000000 - std::chrono::duration_cast<std::chrono::nanoseconds>(before - after).count();
		nanosleep(&ts_in, NULL);
		
		// Record the 'after' time.
		after = std::chrono::high_resolution_clock::now();
		
		// Determine exactly how long we slept for.
		std::chrono::nanoseconds delta = std::chrono::duration_cast<std::chrono::nanoseconds>(after - before);
		
		// Find any times that have expired.
		for (int i = 0; i < MAX_TIMERS; i++) {
			struct runtime_timer *timer = &timers[i];
			if (!timer->valid) continue;
			
			// Is a timer expiring on this tick?  Trigger it.
			if (timer->remaining <= delta) {
				timer->remaining = timer->interval;
				timer->sink->timer_expired((delta / timer->interval) + 1);
			} else {
				// Otherwise, ignore it.
				timer->remaining -= delta;
			}
		}
		
		// Record the 'before' time.
		before = std::chrono::high_resolution_clock::now();
	}
}
