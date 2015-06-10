/*
 * File:   millisecond-tick-source.h
 * Author: spink
 *
 * Created on 18 February 2015, 16:43
 */

#ifndef MILLISECOND_TICK_SOURCE_H
#define	MILLISECOND_TICK_SOURCE_H

#include <devices/timers/tick-source.h>
#include <thread>

namespace captive {
	namespace devices {
		namespace timers {
			class MillisecondTickSource : public TickSource
			{
			public:
				MillisecondTickSource();
				virtual ~MillisecondTickSource();

				virtual void start() override;
				virtual void stop() override;

			private:
				static void tick_thread_proc(MillisecondTickSource *o);
				std::thread *tick_thread;

				volatile bool terminate;
			};
		}
	}
}

#endif	/* MILLISECOND_TICK_SOURCE_H */

