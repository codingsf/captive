/*
 * File:   arm-mmu.h
 * Author: spink
 *
 * Created on 13 February 2015, 18:13
 */

#ifndef ARM_MMU_H
#define	ARM_MMU_H

#include <mmu.h>

namespace captive {
	namespace arch {
		namespace arm {
			class ArmCPU;

			class ArmMMU : public MMU
			{
			public:
				ArmMMU(ArmCPU& cpu);
				~ArmMMU();

				bool enable() override;
				bool disable() override;

				bool enabled() const override {
					return _enabled;
				}

			protected:
				bool resolve_gpa(gva_t va, gpa_t& pa) const override;

			private:
				ArmCPU& _cpu;
				bool _enabled;
			};
		}
	}
}

#endif	/* ARM_MMU_H */

