/*
 * File:   ir-instruction.h
 * Author: spink
 *
 * Created on 27 February 2015, 16:33
 */

#ifndef IR_INSTRUCTION_H
#define	IR_INSTRUCTION_H

namespace captive {
	namespace arch {
		namespace jit {
			struct IROperand
			{
				enum IROperandType {
					CONSTANT,
					VREG
				};

				IROperandType type;
				uint32_t value;
			};

			struct IRInstruction
			{
				enum IRInstructionType {
					NOP,

					ADD,
					SUB,
					MUL,
					DIV
				};

				IRInstructionType type;
				IROperand operands[4];
			};

			class IRInstructionBuilder {
			public:
				static IRInstruction create_nop()
				{
					IRInstruction insn;
					insn.type = IRInstruction::NOP;

					return insn;
				}

				static IRInstruction create_unary(IRInstruction::IRInstructionType type, IROperand op)
				{
					IRInstruction insn;
					insn.type = type;
					insn.operands[0] = op;

					return insn;
				}

				static IRInstruction create_binary(IRInstruction::IRInstructionType type, IROperand src, IROperand dst)
				{
					IRInstruction insn;
					insn.type = type;
					insn.operands[0] = src;
					insn.operands[1] = dst;

					return insn;
				}

				static IRInstruction create_add(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::ADD, src, dst);
				}

				static IRInstruction create_sub(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::SUB, src, dst);
				}

				static IRInstruction create_mul(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::MUL, src, dst);
				}

				static IRInstruction create_div(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::DIV, src, dst);
				}
			};
		}
	}
}

#endif	/* IR_INSTRUCTION_H */

