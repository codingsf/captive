/*
 * File:   engine.h
 * Author: spink
 *
 * Created on 29 January 2015, 08:09
 */

#ifndef ENGINE_H
#define	ENGINE_H

#include <define.h>
#include <map>

namespace captive {
	namespace engine {
		class Engine
		{
		public:
			Engine(std::string libfile);
			virtual ~Engine();

			bool init();
			bool install(uint8_t *base);

			uint64_t entrypoint() const { return _entrypoint; }

			inline bool lookup_symbol(std::string name, uint64_t& symbol) {
				auto sym = symbols.find(name);
				if (sym == symbols.end())
					return false;

				symbol = sym->second;
				return true;
			}

		private:
			bool load();
			bool loaded;

			std::string libfile;
			uint8_t *lib;
			size_t lib_size;

			uint64_t _entrypoint;

			std::map<std::string, uint64_t> symbols;
		};
	}
}

#endif	/* ENGINE_H */
