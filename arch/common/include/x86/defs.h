/*
 * File:   defs.h
 * Author: spink
 *
 * Created on 27 April 2015, 14:37
 */

#ifndef DEFS_H
#define	DEFS_H

namespace captive {
	namespace arch {
		namespace x86 {
			struct Operand
			{
				enum {
					TYPE_IMMEDIATE,
					TYPE_REGISTER,
					TYPE_MEMORY
				} type;

				enum reg_idx {
					R_RAX,
					R_RBX,
					R_RCX,
					R_RDX,
					R_RSP,
					R_RBP,
					R_RSI,
					R_RDI,

					R_EAX,
					R_EBX,
					R_ECX,
					R_EDX,
					R_ESP,
					R_EBP,
					R_ESI,
					R_EDI,

					R_AX,
					R_BX,
					R_CX,
					R_DX,
					R_SP,
					R_BP,
					R_SI,
					R_DI,

					R_AH,
					R_BH,
					R_CH,
					R_DH,
					R_AL,
					R_BL,
					R_CL,
					R_DL,

					R_SPL,
					R_BPL,
					R_SIL,
					R_DIL,

					R_Z,
				};

				union {
					uint64_t immed_val;
					reg_idx reg;
					struct {
						reg_idx base_reg_idx;
						reg_idx index_reg_idx;
						uint8_t scale;
						int32_t displacement;
					} mem;
				};
			};
		}
	}
}

#endif	/* DEFS_H */

