/*
 * File:   microsecond-tick-source.h
 * Author: spink
 *
 * Created on 15 June 2015
 */

#ifndef MICROSECOND_TICK_SOURCE_H
#define	MICROSECOND_TICK_SOURCE_H

#include <devices/timers/tick-source.h>
#include <thread>

namespace captive {
	namespace devices {
		namespace timers {
			class MicrosecondTickSource : public TickSource
			{
			public:
				MicrosecondTickSource();
				virtual ~MicrosecondTickSource();

				virtual void start() override;
				virtual void stop() override;

			private:
				void recalibrate();

				static void tick_thread_proc(MicrosecondTickSource *o);
				std::thread *tick_thread;

				volatile bool terminate;
			};
		}
	}
}

#endif	/* MICROSECOND_TICK_SOURCE_H */

