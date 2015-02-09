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

			uint64_t entrypoint_offset() const { return _entrypoint_offset; }

		private:
			bool load();
			bool loaded;

			std::string libfile;
			uint8_t *lib;
			size_t lib_size;

			uint64_t _entrypoint_offset;
		};
	}
}

#endif	/* ENGINE_H */
