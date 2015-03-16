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
		namespace profile {
			class Image;
		}
		
		namespace arm {
			class ArmEnvironment : public Environment
			{
			public:
				ArmEnvironment(captive::PerCPUData *per_cpu_data);
				virtual ~ArmEnvironment();

				CPU *create_cpu() override;

			private:
				profile::Image *_profile_image;
			};
		}
	}
}

#endif	/* ENV_H */

