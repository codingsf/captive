/*
 * File:   env.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:42
 */

#ifndef ENV_H
#define	ENV_H

#include <define.h>

namespace captive {
	namespace arch {
		class CPU;
		class CoreDevice;

		class Environment
		{
		public:
			Environment();
			virtual ~Environment();

			virtual bool init();
			bool run(unsigned int ep);

			virtual CPU *create_cpu() = 0;

			bool write_core_device(CPU& cpu, uint32_t id, uint32_t reg, uint32_t data);
			bool read_core_device(CPU& cpu, uint32_t id, uint32_t reg, uint32_t& data);

			inline bool install_core_device(uint32_t id, CoreDevice *device) {
				if (devices[id])
					return false;

				devices[id] = device;
				return true;
			}

		private:
			CoreDevice *devices[16];
		};
	}
}

#endif	/* ENV_H */
