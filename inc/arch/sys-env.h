/*
 * File:   sys-env.h
 * Author: spink
 *
 * Created on 10 February 2015, 09:10
 */

#ifndef SYS_ENV_H
#define	SYS_ENV_H

#include <define.h>

namespace captive {
	namespace arch {
		class CpuEnvironment;

		class SystemEnvironment
		{
		public:
			SystemEnvironment();
			virtual ~SystemEnvironment();
			
			virtual CpuEnvironment *create_cpu() = 0;
			virtual uint32_t size() const = 0;
		};
	}
}

#endif	/* SYS_ENV_H */

