/*
 * File:   coco.h
 * Author: spink
 *
 * Created on 12 February 2015, 11:43
 */

#ifndef COCO_H
#define	COCO_H

#include <devices/coprocessor.h>

namespace captive {
	namespace arch {
		namespace arm {
			namespace devices {
				class CoCo : public Coprocessor
				{
				public:
					CoCo(Environment& env);
					~CoCo();

					inline uint32_t TTBR0() const { return _TTBR0; }
					inline uint32_t TTBR1() const { return _TTBR1; }

					inline uint32_t FSR() const { return _FSR; }
					inline void FSR(uint32_t v) { _FSR = v; }
					inline uint32_t FAR() const { return _FAR; }
					inline void FAR(uint32_t v) { _FAR = v; }

				protected:
					bool mcr(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data) override;
					bool mrc(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data) override;

				private:
					bool L2;
					bool EE;
					bool VE;
					bool XP;
					bool U;
					bool FI;
					bool L4;
					bool RR;
					bool I;
					bool Z;
					bool F;
					bool R;
					bool S;
					bool B;
					bool L;
					bool D;
					bool P;
					bool W;
					bool C;
					bool A;
					bool M;

					uint32_t _TTBR0, _TTBR1;
					uint32_t _DACR, _FSR, _FAR;
				};
			}
		}
	}
}

#endif	/* COCO_H */

