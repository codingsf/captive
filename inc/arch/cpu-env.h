/*
 * File:   cpu-env.h
 * Author: spink
 *
 * Created on 10 February 2015, 09:10
 */

#ifndef CPU_ENV_H
#define	CPU_ENV_H

#include <define.h>

namespace captive {
	namespace arch {
		class SystemEnvironment;

		class CpuEnvironment
		{
		public:
			CpuEnvironment(SystemEnvironment& system);
			virtual ~CpuEnvironment();
			
			virtual uint32_t size() const = 0;

			inline SystemEnvironment& system() const { return _system; }

		private:
			SystemEnvironment& _system;
		};
	}
}

#endif	/* CPU_ENV_H */

