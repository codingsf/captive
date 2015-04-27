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

				static inline int register_size(reg_idx rgid)
				{
					switch (rgid) {
					case R_RAX: case R_RBX: case R_RCX: case R_RDX:
					case R_RSP: case R_RBP: case R_RSI: case R_RDI:
						return 8;
					case R_EAX: case R_EBX: case R_ECX: case R_EDX:
					case R_ESP: case R_EBP: case R_ESI: case R_EDI:
						return 4;
					default:
						return 0;
					}
				}

				static inline int register_to_index(reg_idx rgid)
				{
					switch (rgid) {
					case R_RAX: case R_EAX: case R_AX: case R_AL: return 0;
					case R_RCX: case R_ECX: case R_CX: case R_CL: return 1;
					case R_RDX: case R_EDX: case R_DX: case R_DL: return 2;
					case R_RBX: case R_EBX: case R_BX: case R_BL: return 3;
					case R_RSP: case R_ESP: case R_SP: case R_SPL: return 4;
					case R_RBP: case R_EBP: case R_BP: case R_BPL: return 5;
					case R_RSI: case R_ESI: case R_SI: case R_SIL: return 6;
					case R_RDI: case R_EDI: case R_DI: case R_DIL: return 7;

					default: return -1;
					}
				}
			};
		}
	}
}

#endif	/* DEFS_H */

