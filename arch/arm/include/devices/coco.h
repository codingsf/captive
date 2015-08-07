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

					inline bool R() const { return _R; }
					inline bool S() const { return _S; }

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
					bool _R;
					bool _S;
					bool B;
					bool L;
					bool D;
					bool P;
					bool W;
					bool C;
					bool A;
					bool M;
				};
			}
		}
	}
}

#endif	/* COCO_H */

