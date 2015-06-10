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
				CallbackTickSource(uint32_t scale);
				virtual ~CallbackTickSource();

				void do_tick() { if (_current_scale-- == 0) { this->tick(1); _current_scale = _scale; } }

			private:
				uint32_t _scale;
				uint32_t _current_scale;
			};
		}
	}
}

#endif	/* CALLBACK_TICK_SOURCE_H */

