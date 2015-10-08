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

					inline bool R() const { return false; }
					inline bool S() const { return false; }
					inline bool A() const { return _A; }

				protected:
					bool mcr(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data) override;
					bool mrc(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data) override;

				private:
					bool _A, _C, _Z, _I, _EE, _TRE, _AFE, _TE;
					
					uint32_t DATA_TCM_REGION;
					uint32_t INSN_TCM_REGION;
					uint32_t CACHE_SIZE_SELECTION;
					uint32_t PRIMARY_REGION_REMAP;
					uint32_t NORMAL_REGION_REMAP;
					uint32_t CONTEXT_ID;
				};
			}
		}
	}
}

#endif	/* COCO_H */

