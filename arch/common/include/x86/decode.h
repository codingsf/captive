/*
 * File:   decode.h
 * Author: spink
 *
 * Created on 22 April 2015, 11:51
 */

#ifndef DECODE_H
#define	DECODE_H

#include <define.h>

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

			struct MemoryInstruction
			{
				uint8_t length;
				uint8_t data_size;
				
				enum {
					I_MOV,
					I_MOVZX,
				} type;

				struct Operand Source;
				struct Operand Dest;
			};

			bool decode_memory_instruction(const uint8_t *code, MemoryInstruction& inst);
		}
	}
}

#endif	/* DECODE_H */

