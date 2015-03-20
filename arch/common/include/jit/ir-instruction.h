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
					NONE,
					CONSTANT,
					VREG,
					BLOCK,
					FUNC,
					PC
				};

				IROperandType type;
				uint64_t value;
				uint8_t size;

				inline bool constant() const { return type == CONSTANT; }
				inline bool vreg() const { return type == VREG; }
				inline bool block() const { return type == BLOCK; }
				inline bool func() const { return type == FUNC; }
				inline bool pc() const { return type == PC; }
			} packed;

			struct IRInstruction
			{
				enum IRInstructionType {
					INVALID,

					VERIFY,

					NOP,
					TRAP,

					MOV,
					CMOV,
					LDPC,

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

					CALL,
					JMP,
					BRANCH,
					RET,

					SET_CPU_MODE,
					WRITE_DEVICE,
					READ_DEVICE,
				};

				IRInstructionType type;
				IROperand operands[6];

				IRInstruction() : type(INVALID) {
					operands[0].type = IROperand::NONE;
					operands[1].type = IROperand::NONE;
					operands[2].type = IROperand::NONE;
					operands[3].type = IROperand::NONE;
					operands[4].type = IROperand::NONE;
					operands[5].type = IROperand::NONE;
				}
			} packed;

			class IRInstructionBuilder {
			public:
				static IROperand create_constant(uint8_t size, uint64_t val)
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

				static IROperand create_vreg(reg_id_t id, uint8_t size)
				{
					IROperand oper;
					oper.type = IROperand::VREG;
					oper.value = id;
					oper.size = size;

					return oper;
				}

				static IROperand create_pc(uint32_t offset)
				{
					IROperand oper;
					oper.type = IROperand::PC;
					oper.value = offset;
					oper.size = 4;

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

				static IROperand create_func(void *addr)
				{
					IROperand oper;
					oper.type = IROperand::FUNC;
					oper.value = (uint64_t)addr;
					oper.size = 0;

					return oper;
				}

				static IRInstruction create_nop()
				{
					IRInstruction insn;
					insn.type = IRInstruction::NOP;

					return insn;
				}

				static IRInstruction create_ret()
				{
					IRInstruction insn;
					insn.type = IRInstruction::RET;

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

				static IRInstruction create_verify()
				{
					IRInstruction insn;
					insn.type = IRInstruction::VERIFY;

					return insn;
				}

				static IRInstruction create_ldpc(IROperand dst)
				{
					assert(dst.vreg());

					return create_unary(IRInstruction::LDPC, dst);
				}

				static IRInstruction create_mov(IROperand src, IROperand dst)
				{
					assert(src.size == dst.size);
					assert(src.constant() || src.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::MOV, src, dst);
				}

				static IRInstruction create_cmov(IROperand cond, IROperand src, IROperand dst)
				{
					assert(src.size == dst.size);
					assert(src.constant() || src.vreg());
					assert(dst.vreg());

					return create_ternary(IRInstruction::CMOV, cond, src, dst);
				}

				static IRInstruction create_add(IROperand src, IROperand dst)
				{
					assert(src.size == dst.size);
					assert(src.constant() || src.vreg() || src.pc());
					assert(dst.vreg());

					return create_binary(IRInstruction::ADD, src, dst);
				}

				static IRInstruction create_sub(IROperand src, IROperand dst)
				{
					assert(src.size == dst.size);
					assert(src.constant() || src.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::SUB, src, dst);
				}

				static IRInstruction create_mul(IROperand src, IROperand dst)
				{
					assert(src.size == dst.size);
					assert(src.constant() || src.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::MUL, src, dst);
				}

				static IRInstruction create_div(IROperand src, IROperand dst)
				{
					assert(src.size == dst.size);
					assert(src.constant() || src.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::DIV, src, dst);
				}

				static IRInstruction create_mod(IROperand src, IROperand dst)
				{
					assert(src.size == dst.size);
					assert(src.constant() || src.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::MOD, src, dst);
				}

				static IRInstruction create_shl(IROperand amt, IROperand dst)
				{
					assert(amt.constant() || amt.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::SHL, amt, dst);
				}

				static IRInstruction create_shr(IROperand amt, IROperand dst)
				{
					assert(amt.constant() || amt.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::SHR, amt, dst);
				}

				static IRInstruction create_sar(IROperand amt, IROperand dst)
				{
					assert(amt.constant() || amt.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::SAR, amt, dst);
				}

				static IRInstruction create_clz(IROperand src, IROperand dst)
				{
					return create_binary(IRInstruction::CLZ, src, dst);
				}

				static IRInstruction create_sx(IROperand src, IROperand dst)
				{
					assert(dst.size > src.size);
					assert(dst.vreg());

					return create_binary(IRInstruction::SX, src, dst);
				}

				static IRInstruction create_zx(IROperand src, IROperand dst)
				{
					assert(dst.size > src.size);
					assert(dst.vreg());

					return create_binary(IRInstruction::ZX, src, dst);
				}

				static IRInstruction create_trunc(IROperand src, IROperand dst)
				{
					assert(dst.size < src.size);
					assert(dst.vreg());

					return create_binary(IRInstruction::TRUNC, src, dst);
				}

				static IRInstruction create_and(IROperand src, IROperand dst)
				{
					assert(src.size == dst.size);
					assert(src.constant() || src.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::AND, src, dst);
				}

				static IRInstruction create_or(IROperand src, IROperand dst)
				{
					assert(src.size == dst.size);
					assert(src.constant() || src.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::OR, src, dst);
				}

				static IRInstruction create_xor(IROperand src, IROperand dst)
				{
					assert(src.size == dst.size);
					assert(src.constant() || src.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::XOR, src, dst);
				}

				static IRInstruction create_cmpeq(IROperand lh, IROperand rh, IROperand dst)
				{
					assert(lh.size == rh.size);
					assert(lh.constant() || lh.vreg());
					assert(rh.constant() || rh.vreg());
					assert(dst.vreg());

					return create_ternary(IRInstruction::CMPEQ, lh, rh, dst);
				}

				static IRInstruction create_cmpne(IROperand lh, IROperand rh, IROperand dst)
				{
					assert(lh.size == rh.size);
					assert(lh.constant() || lh.vreg());
					assert(rh.constant() || rh.vreg());
					assert(dst.vreg());

					return create_ternary(IRInstruction::CMPNE, lh, rh, dst);
				}

				static IRInstruction create_cmpgt(IROperand lh, IROperand rh, IROperand dst)
				{
					assert(lh.size == rh.size);
					assert(lh.constant() || lh.vreg());
					assert(rh.constant() || rh.vreg());
					assert(dst.vreg());

					return create_ternary(IRInstruction::CMPGT, lh, rh, dst);
				}

				static IRInstruction create_cmpgte(IROperand lh, IROperand rh, IROperand dst)
				{
					assert(lh.size == rh.size);
					assert(lh.constant() || lh.vreg());
					assert(rh.constant() || rh.vreg());
					assert(dst.vreg());

					return create_ternary(IRInstruction::CMPGTE, lh, rh, dst);
				}

				static IRInstruction create_cmplt(IROperand lh, IROperand rh, IROperand dst)
				{
					assert(lh.size == rh.size);
					assert(lh.constant() || lh.vreg());
					assert(rh.constant() || rh.vreg());
					assert(dst.vreg());

					return create_ternary(IRInstruction::CMPLT, lh, rh, dst);
				}

				static IRInstruction create_cmplte(IROperand lh, IROperand rh, IROperand dst)
				{
					assert(lh.size == rh.size);
					assert(lh.constant() || lh.vreg());
					assert(rh.constant() || rh.vreg());
					assert(dst.vreg());

					return create_ternary(IRInstruction::CMPLTE, lh, rh, dst);
				}

				static IRInstruction create_ldreg(IROperand loc, IROperand dst)
				{
					assert(loc.constant() || loc.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::READ_REG, loc, dst);
				}

				static IRInstruction create_streg(IROperand val, IROperand loc)
				{
					assert(val.constant() || val.vreg() || val.pc());
					assert(loc.constant() || loc.vreg());

					return create_binary(IRInstruction::WRITE_REG, val, loc);
				}

				static IRInstruction create_ldmem(IROperand loc, IROperand dst)
				{
					assert(loc.constant() || loc.vreg());
					assert(dst.vreg());

					return create_binary(IRInstruction::READ_MEM, loc, dst);
				}

				static IRInstruction create_stmem(IROperand val, IROperand loc)
				{
					assert(val.constant() || val.vreg());
					assert(loc.constant() || loc.vreg());

					return create_binary(IRInstruction::WRITE_MEM, val, loc);
				}

				static IRInstruction create_jump(IROperand dest)
				{
					assert(dest.block());
					return create_unary(IRInstruction::JMP, dest);
				}

				static IRInstruction create_branch(IROperand cond, IROperand td, IROperand fd)
				{
					assert(td.block() && fd.block());
					return create_ternary(IRInstruction::BRANCH, cond, td, fd);
				}

				static IRInstruction create_call0(IROperand fn)
				{
					assert(fn.func());

					IRInstruction insn;
					insn.type = IRInstruction::CALL;
					insn.operands[0] = fn;

					return insn;
				}

				static IRInstruction create_call1(IROperand fn, IROperand arg0)
				{
					assert(fn.func());

					IRInstruction insn;
					insn.type = IRInstruction::CALL;
					insn.operands[0] = fn;
					insn.operands[1] = arg0;
					return insn;
				}

				static IRInstruction create_call2(IROperand fn, IROperand arg0, IROperand arg1)
				{
					assert(fn.func());

					IRInstruction insn;
					insn.type = IRInstruction::CALL;
					insn.operands[0] = fn;
					insn.operands[1] = arg0;
					insn.operands[2] = arg1;
					return insn;
				}

				static IRInstruction create_call3(IROperand fn, IROperand arg0, IROperand arg1, IROperand arg2)
				{
					assert(fn.func());

					IRInstruction insn;
					insn.type = IRInstruction::CALL;
					insn.operands[0] = fn;
					insn.operands[1] = arg0;
					insn.operands[2] = arg1;
					insn.operands[3] = arg2;
					return insn;
				}

				static IRInstruction create_call4(IROperand fn, IROperand arg0, IROperand arg1, IROperand arg2, IROperand arg3)
				{
					assert(fn.func());

					IRInstruction insn;
					insn.type = IRInstruction::CALL;
					insn.operands[0] = fn;
					insn.operands[1] = arg0;
					insn.operands[2] = arg1;
					insn.operands[3] = arg2;
					insn.operands[4] = arg3;
					return insn;
				}

				static IRInstruction create_call5(IROperand fn, IROperand arg0, IROperand arg1, IROperand arg2, IROperand arg3, IROperand arg4)
				{
					assert(fn.func());

					IRInstruction insn;
					insn.type = IRInstruction::CALL;
					insn.operands[0] = fn;
					insn.operands[1] = arg0;
					insn.operands[2] = arg1;
					insn.operands[3] = arg2;
					insn.operands[4] = arg3;
					insn.operands[5] = arg4;

					return insn;
				}

				static IRInstruction create_set_cpu_mode(IROperand mode)
				{
					assert(mode.vreg() || mode.constant());
					assert(mode.size == 1);
					return create_unary(IRInstruction::SET_CPU_MODE, mode);
				}

				static IRInstruction create_write_device(IROperand dev, IROperand reg, IROperand val)
				{
					assert(dev.vreg() || dev.constant());
					assert(reg.vreg() || reg.constant());
					assert(val.vreg() || val.constant());

					return create_ternary(IRInstruction::WRITE_DEVICE, dev, reg, val);
				}

				static IRInstruction create_read_device(IROperand dev, IROperand reg, IROperand dst)
				{
					assert(dev.vreg() || dev.constant());
					assert(reg.vreg() || reg.constant());
					assert(dst.vreg());

					return create_ternary(IRInstruction::READ_DEVICE, dev, reg, dst);
				}
			};
		}
	}
}

#endif	/* IR_INSTRUCTION_H */

