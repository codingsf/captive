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

			virtual void *get_bootloader_addr() const = 0;
			virtual uint64_t get_bootloader_size() const = 0;
		};
	}
}

#endif	/* ENGINE_H */

