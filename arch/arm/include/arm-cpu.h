/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:38
 */

#ifndef ARMCPU_H
#define	ARMCPU_H

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

			private:
				unsigned int _ep;
			};
		}
	}
}

#endif	/* CPU_H */

