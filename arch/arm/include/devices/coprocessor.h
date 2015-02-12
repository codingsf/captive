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
				class Coprocessor : public Device
				{
				public:
					Coprocessor(Environment& env);
					virtual ~Coprocessor();

					bool read(uint32_t reg, uint32_t& data) override;
					bool write(uint32_t reg, uint32_t data) override;

				protected:
					virtual bool mcr(uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data) = 0;
					virtual bool mrc(uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data) = 0;

				private:
					uint32_t op1, op2, rn, rm;
				};
			}
		}
	}
}

#endif	/* COPROCESSOR_H */
