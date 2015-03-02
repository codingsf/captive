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
				uint8_t size;
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
				static IROperand create_constant(uint8_t size, uint32_t val)
				{
					IROperand oper;
					oper.type = IROperand::CONSTANT;
					oper.value = val;
					oper.size = size;

					return oper;
				}

				static IROperand create_constant32(uint32_t val)
				{
					return create_constant(4, val);
				}

				static IROperand create_constant16(uint32_t val)
				{
					return create_constant(2, val);
				}

				static IROperand create_constant8(uint32_t val)
				{
					return create_constant(1, val);
				}

				static IROperand create_vreg(uint32_t id)
				{
					IROperand oper;
					oper.type = IROperand::VREG;
					oper.value = id;
					oper.size = 4;

					return oper;
				}

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

