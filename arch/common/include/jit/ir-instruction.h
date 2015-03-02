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
			typedef uint32_t block_id_t;
			typedef uint32_t reg_id_t;

			struct IROperand
			{
				enum IROperandType {
					CONSTANT,
					VREG,
					BLOCK
				};

				IROperandType type;
				uint64_t value;
				uint8_t size;
			};

			struct IRInstruction
			{
				enum IRInstructionType {
					NOP,
					TRAP,

					MOV,
					CMOV,

					ADD,
					SUB,
					MUL,
					DIV,
					MOD,

					SHL,
					SHR,
					SAR,
					CLZ,

					AND,
					OR,
					XOR,

					CMPEQ,
					CMPNE,
					CMPGT,
					CMPGTE,
					CMPLT,
					CMPLTE,

					SX,
					ZX,
					TRUNC,

					READ_REG,
					WRITE_REG,
					READ_MEM,
					WRITE_MEM,

					JMP,
					BRANCH,

					TAKE_EXCEPTION,
					SET_CPU_MODE,
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

				static IROperand create_constant64(uint64_t val)
				{
					return create_constant(8, val);
				}

				static IROperand create_constant32(uint32_t val)
				{
					return create_constant(4, val);
				}

				static IROperand create_constant16(uint16_t val)
				{
					return create_constant(2, val);
				}

				static IROperand create_constant8(uint8_t val)
				{
					return create_constant(1, val);
				}

				static IROperand create_vreg(reg_id_t id)
				{
					IROperand oper;
					oper.type = IROperand::VREG;
					oper.value = id;
					oper.size = 0;

					return oper;
				}

				static IROperand create_block_operand(block_id_t id)
				{
					IROperand oper;
					oper.type = IROperand::BLOCK;
					oper.value = id;
					oper.size = 0;

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

				static IRInstruction create_ternary(IRInstruction::IRInstructionType type, IROperand three, IROperand src, IROperand dst)
				{
					IRInstruction insn;
					insn.type = type;
					insn.operands[0] = three;
					insn.operands[1] = src;
					insn.operands[2] = dst;

					return insn;
				}

				static IRInstruction create_trap()
				{
					IRInstruction insn;
					insn.type = IRInstruction::TRAP;
					return insn;
				}

				static IRInstruction create_mov(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::MOV, src, dst);
				}

				static IRInstruction create_cmov(IROperand cond, IROperand src, IROperand dst)
				{
					return create_ternary(IRInstruction::CMOV, cond, src, dst);
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

				static IRInstruction create_mod(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::MOD, src, dst);
				}

				static IRInstruction create_shl(IROperand amt, IROperand dst)
				{
					return create_binary(IRInstruction::SHL, amt, dst);
				}

				static IRInstruction create_shr(IROperand amt, IROperand dst)
				{
					return create_binary(IRInstruction::SHR, amt, dst);
				}

				static IRInstruction create_sar(IROperand amt, IROperand dst)
				{
					return create_binary(IRInstruction::SAR, amt, dst);
				}

				static IRInstruction create_clz(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::CLZ, src, dst);
				}

				static IRInstruction create_sx(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::SX, src, dst);
				}

				static IRInstruction create_zx(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::ZX, src, dst);
				}

				static IRInstruction create_trunc(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::TRUNC, src, dst);
				}

				static IRInstruction create_and(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::AND, src, dst);
				}

				static IRInstruction create_or(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::OR, src, dst);
				}

				static IRInstruction create_xor(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::XOR, src, dst);
				}

				static IRInstruction create_cmpeq(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::CMPEQ, src, dst);
				}

				static IRInstruction create_cmpne(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::CMPNE, src, dst);
				}

				static IRInstruction create_cmpgt(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::CMPGT, src, dst);
				}

				static IRInstruction create_cmpgte(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::CMPGTE, src, dst);
				}

				static IRInstruction create_cmplt(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::CMPLT, src, dst);
				}

				static IRInstruction create_cmplte(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::CMPLTE, src, dst);
				}

				static IRInstruction create_ldreg(IROperand loc, IROperand dst)
				{
					return create_binary(IRInstruction::READ_REG, loc, dst);
				}

				static IRInstruction create_streg(IROperand val, IROperand loc)
				{
					return create_binary(IRInstruction::WRITE_REG, val, loc);
				}

				static IRInstruction create_ldmem(IROperand loc, IROperand dst)
				{
					return create_binary(IRInstruction::READ_MEM, loc, dst);
				}

				static IRInstruction create_stmem(IROperand val, IROperand loc)
				{
					return create_binary(IRInstruction::WRITE_MEM, val, loc);
				}

				static IRInstruction create_jump(IROperand dest)
				{
					return create_unary(IRInstruction::JMP, dest);
				}

				static IRInstruction create_branch(IROperand cond, IROperand td, IROperand fd)
				{
					return create_ternary(IRInstruction::BRANCH, cond, td, fd);
				}

				static IRInstruction create_take_exception(IROperand code, IROperand data)
				{
					return create_binary(IRInstruction::TAKE_EXCEPTION, code, data);
				}

				static IRInstruction create_set_cpu_mode(IROperand mode)
				{
					return create_unary(IRInstruction::SET_CPU_MODE, mode);
				}
			};
		}
	}
}

#endif	/* IR_INSTRUCTION_H */

