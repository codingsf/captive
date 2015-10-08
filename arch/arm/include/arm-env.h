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
			class arm_environment : public Environment
			{
			public:
				enum arm_variant
				{
					ARMv5,
					ARMv6,
					ARMv7
				};

				arm_environment(arm_variant variant, captive::PerCPUData *per_cpu_data);
				virtual ~arm_environment();

				CPU *create_cpu() override;
				
				inline arm_variant variant() const { return _variant; }

			protected:
				bool prepare_boot_cpu(CPU *core) override;
				bool prepare_bootloader() override;
				
			private:
				arm_variant _variant;
			};
		}
	}
}

#endif	/* ENV_H */
