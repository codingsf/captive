/* 
 * File:   debug-coprocessor.h
 * Author: s0457958
 */

#ifndef CAPTIVE_COPROCESSOR_H
#define CAPTIVE_COPROCESSOR_H

#include <devices/coprocessor.h>

namespace captive {
	namespace arch {
		namespace arm {
			namespace devices {
				class CaptiveCoprocessor : public Coprocessor
				{
				public:
					CaptiveCoprocessor(Environment& env);
					~CaptiveCoprocessor();
					
				protected:
					bool mcr(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data) override;
					bool mrc(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data) override;
				};
			}
		}
	}
}

#endif /* CAPTIVE_COPROCESSOR_H */

