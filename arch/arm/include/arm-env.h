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
				enum arch_variant
				{
					ARMv5,
					ARMv6,
					ARMv7
				};
				
				enum core_variant
				{
					CortexA8,
					CortexA9,
				};

				arm_environment(arch_variant arch_variant, core_variant core_variant, captive::PerCPUData *per_cpu_data);
				virtual ~arm_environment();

				CPU *create_cpu() override;
				
				inline arch_variant arch_variant() const { return _arch_variant; }
				inline core_variant core_variant() const { return _core_variant; }

			protected:
				bool prepare_boot_cpu(CPU *core) override;
				bool prepare_bootloader() override;
				
			private:
				enum arch_variant _arch_variant;
				enum core_variant _core_variant;
			};
		}
	}
}

#endif	/* ENV_H */
