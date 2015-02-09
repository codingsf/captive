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
			Engine(std::string libfile);
			virtual ~Engine();

			bool init();
			bool install(uint8_t *base);

		private:
			bool load();
			bool loaded;

			std::string libfile;
			void *lib;
			size_t lib_size;
		};
	}
}

#endif	/* ENGINE_H */
