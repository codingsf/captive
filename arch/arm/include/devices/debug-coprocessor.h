/* 
 * File:   debug-coprocessor.h
 * Author: s0457958
 *
 * Created on 24 September 2015, 11:57
 */

#ifndef DEBUG_COPROCESSOR_H
#define DEBUG_COPROCESSOR_H

#include <devices/coprocessor.h>

namespace captive {
	namespace arch {
		namespace arm {
			namespace devices {
				class DebugCoprocessor : public Coprocessor
				{
				public:
					DebugCoprocessor(Environment& env);
					~DebugCoprocessor();
					
				protected:
					bool mcr(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data) override;
					bool mrc(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data) override;
				};
			}
		}
	}
}

#endif /* DEBUG_COPROCESSOR_H */

