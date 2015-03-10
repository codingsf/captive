/*
 * File:   wsj-x86.h
 * Author: spink
 *
 * Created on 10 March 2015, 16:51
 */

#ifndef WSJ_X86_H
#define	WSJ_X86_H

#include <define.h>

#include <list>
#include <vector>

namespace captive {
	namespace jit {
		namespace x86 {
			class X86Operand
			{
			public:
				enum X86OperandType
				{
					REGISTER,
					MEMORY,
					IMMEDIATE
				};

				enum X86Register
				{
					RAX,
					RBX,
					RCX,
					RDX,
					RSI,
					RDI
				};

				X86Operand(X86OperandType type) : type(type) { }

				X86OperandType type;

				uint8_t operand_size;

				union {
					uint64_t immediate_value;
					X86Register reg;
					struct {
						X86Register base;
						uint32_t displacement;
					};
				};
			};

			class X86Builder;
			class X86Instruction
			{
			public:
				enum X86InstructionOpcode
				{
					RET = 0xc3
				};

				X86Instruction(X86InstructionOpcode opcode) : opcode(opcode) { }

				inline void add_operand(X86Operand op)
				{
					operands.push_back(op);
				}

				X86InstructionOpcode opcode;
				std::list<X86Operand> operands;
			};

			class X86Builder
			{
			public:
				X86Builder();

				bool generate(void *buffer, uint64_t& size);

				inline void ret() {
					add_instruction(X86Instruction(X86Instruction::RET));
				}

			private:
				std::list<X86Instruction> instructions;

				inline void add_instruction(X86Instruction insn)
				{
					instructions.push_back(insn);
				}
			};
		}
	}
}

#endif	/* WSJ_X86_H */

