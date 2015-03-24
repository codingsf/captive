/*
 * File:   callback-tick-source.h
 * Author: spink
 *
 * Created on 23 March 2015, 14:26
 */

#ifndef CALLBACK_TICK_SOURCE_H
#define	CALLBACK_TICK_SOURCE_H


#include <devices/timers/tick-source.h>
#include <thread>

namespace captive {
	namespace devices {
		namespace timers {
			class CallbackTickSource : public TickSource
			{
			public:
				CallbackTickSource();
				virtual ~CallbackTickSource();

				void do_tick() { this->tick(1); }
			};
		}
	}
}

#endif	/* CALLBACK_TICK_SOURCE_H */

