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
		namespace armv5e {
			class armv5e_environment : public Environment
			{
			public:
				armv5e_environment(captive::PerCPUData *per_cpu_data);
				virtual ~armv5e_environment();

				CPU *create_cpu() override;
			};
		}
	}
}

#endif	/* ENV_H */

