/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:38
 */

#ifndef ARMCPU_H
#define	ARMCPU_H

#include <define.h>
#include <cpu.h>

namespace captive {
	namespace arch {
		namespace arm {
			class ArmEnvironment;

			class ArmCPU : public CPU
			{
			public:
				ArmCPU(ArmEnvironment& env);
				virtual ~ArmCPU();

				bool init(unsigned int ep) override;
				bool run() override;

				virtual uint32_t read_pc() const override { return regs.RB[15]; }

			private:
				unsigned int _ep;

				struct {
					uint32_t RB[16];
				} regs;
			};
		}
	}
}

#endif	/* CPU_H */

