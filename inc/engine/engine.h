/*
 * File:   engine.h
 * Author: spink
 *
 * Created on 29 January 2015, 08:09
 */

#ifndef ENGINE_H
#define	ENGINE_H

#include <define.h>

namespace captive {
	namespace engine {
		class Engine
		{
		public:
			explicit Engine();
			virtual ~Engine();

			bool install(uint8_t *base);
		};
	}
}

#endif	/* ENGINE_H */
