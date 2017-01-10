/* 
 * File:   aarch64-env.h
 * Author: s0457958
 *
 * Created on 12 September 2016, 12:52
 */

#ifndef AARCH64_ENV_H
#define AARCH64_ENV_H


#include <env.h>

namespace captive {
	namespace arch {
		namespace aarch64 {
			class aarch64_environment : public Environment
			{
			public:
				enum core_variant
				{
					CortexA72,
				};

				aarch64_environment(core_variant core_variant, captive::PerGuestData *per_guest_data);
				virtual ~aarch64_environment();

				CPU *create_cpu(captive::PerCPUData *per_cpu_data) override;
				
				inline core_variant core_variant() const { return _core_variant; }

			protected:
				bool prepare_boot_cpu(CPU *core) override;
				bool prepare_bootloader() override;
				
			private:
				enum core_variant _core_variant;
			};
		}
	}
}

#endif /* AARCH64_ENV_H */

