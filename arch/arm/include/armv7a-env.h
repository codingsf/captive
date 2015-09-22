/*
 * File:   env.h
 * Author: spink
 *
 * Created on 10 February 2015, 08:18
 */

#ifndef ARMENV_H
#define	ARMENV_H

#include <env.h>

namespace captive {
	namespace arch {
		namespace armv7a {
			class armv7a_environment : public Environment
			{
			public:
				armv7a_environment(captive::PerCPUData *per_cpu_data);
				virtual ~armv7a_environment();

				CPU *create_cpu() override;
			};
		}
	}
}

#endif	/* ENV_H */

