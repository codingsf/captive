/*
 * File:   tick-source.h
 * Author: spink
 *
 * Created on 18 February 2015, 16:41
 */

#ifndef TICK_SOURCE_H
#define	TICK_SOURCE_H

#include <define.h>
#include <list>

namespace captive {
	namespace devices {
		namespace timers {
			class TickSink
			{
			public:
				virtual void tick(uint32_t period) = 0;
			};

			class TickSource
			{
			public:
				TickSource();
				virtual ~TickSource();

				virtual void start();
				virtual void stop();

				inline void add_sink(TickSink& sink) {
					sinks.push_back(&sink);
				}

			protected:
				void tick(uint32_t period);

			private:
				std::list<TickSink *> sinks;
			};
		}
	}
}

#endif	/* TICK_SOURCE_H */

