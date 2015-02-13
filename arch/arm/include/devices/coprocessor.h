/*
 * File:   coprocessor.h
 * Author: spink
 *
 * Created on 12 February 2015, 11:39
 */

#ifndef COPROCESSOR_H
#define	COPROCESSOR_H

#include <device.h>

namespace captive {
	namespace arch {
		namespace arm {
			namespace devices {
				class Coprocessor : public CoreDevice
				{
				public:
					Coprocessor(Environment& env);
					virtual ~Coprocessor();

					bool read(CPU& cpu, uint32_t reg, uint32_t& data) override;
					bool write(CPU& cpu, uint32_t reg, uint32_t data) override;

				protected:
					virtual bool mcr(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data) = 0;
					virtual bool mrc(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data) = 0;

				private:
					uint32_t op1, op2, rn, rm;
				};
			}
		}
	}
}

#endif	/* COPROCESSOR_H */
