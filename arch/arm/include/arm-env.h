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
		namespace arm {
			class ArmEnvironment : public Environment
			{
			public:
				ArmEnvironment();
				virtual ~ArmEnvironment();

				CPU *create_cpu() override;
			};
		}
	}
}

#endif	/* ENV_H */

