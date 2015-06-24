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
				static void tick_thread_proc_tramp(MicrosecondTickSource *o);
				void tick_thread_proc();
				std::thread *tick_thread;

				volatile bool terminate;
				
				uint32_t ticks, total_overshoot, overshoot_samples, calibrated_ticks, recalibrate;
			};
		}
	}
}

#endif	/* MICROSECOND_TICK_SOURCE_H */

