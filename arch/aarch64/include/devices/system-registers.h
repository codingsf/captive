/* 
 * File:   system-registers.h
 * Author: s0457958
 *
 * Created on 15 September 2016, 16:26
 */

#ifndef SYSTEM_REGISTERS_H
#define SYSTEM_REGISTERS_H

#include <device.h>

namespace captive {
	namespace arch {
		namespace aarch64 {
			namespace devices {
				class SystemRegisters : public CoreDevice
				{
				public:
					SystemRegisters(Environment& env);
					virtual ~SystemRegisters();

					bool read(CPU& cpu, uint32_t reg, uint32_t& data) override;
					bool write(CPU& cpu, uint32_t reg, uint32_t data) override;
					
				private:
					struct register_access
					{
						uint8_t op0, op1, op2;
						uint8_t crn, crm;
					};
					
					register_access decode_access(uint32_t ir);
				};
			}
		}
	}
}

#endif /* SYSTEM_REGISTERS_H */

