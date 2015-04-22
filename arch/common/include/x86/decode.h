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
				};

				union {
					uint64_t immed_val;
					reg_idx reg;
					struct {
						reg_idx base_reg_idx;
						uint32_t displacement;
					} mem;
				};
			};

			struct MemoryInstruction
			{
				uint8_t length;
				
				enum {
					I_MOV,
				} type;

				struct Operand Source;
				struct Operand Dest;
			};

			bool decode_memory_instruction(const uint8_t *code, MemoryInstruction& inst);
		}
	}
}

#endif	/* DECODE_H */

