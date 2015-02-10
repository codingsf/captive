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
	namespace arch {
		class Architecture;
	}

	namespace engine {
		class Engine
		{
		public:
			Engine(std::string libfile);
			virtual ~Engine();

			bool init();
			bool install(uint8_t *base);

			inline arch::Architecture& arch() const { return *_arch; }

			uint64_t entrypoint_offset() const { return _entrypoint_offset; }

		private:
			bool load();
			bool loaded;

			std::string libfile;
			uint8_t *lib;
			size_t lib_size;

			arch::Architecture *_arch;

			uint64_t _entrypoint_offset;
		};
	}
}

#endif	/* ENGINE_H */
