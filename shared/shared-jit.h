/*
 * File:   shared-jit.h
 * Author: spink
 *
 * Created on 27 March 2015, 17:49
 */

#ifndef SHARED_JIT_H
#define	SHARED_JIT_H

#ifndef __packed
#ifndef packed
#define __packed __attribute__((packed))
#else
#define __packed packed
#endif
#endif

#include <shmem.h>

namespace captive {
	namespace shared {
		typedef uint32_t IRBlockId;
		typedef uint32_t IRRegId;
		
		struct IRInstruction;
		
		typedef uint32_t (*block_txln_fn)(void *);
		typedef uint32_t (*region_txln_fn)(void *);
		
		struct BlockWorkUnit
		{
			uint32_t offset;
			const IRInstruction *ir;
			unsigned int ir_count;
		};
		
		struct RegionWorkUnit
		{
			uint32_t region_index;
			uint32_t valid;
			
			BlockWorkUnit *blocks;
			unsigned int block_count;
			
			void *fn_ptr;
		};
		
		struct RegionImage
		{
			RegionWorkUnit *rwu;
		};
		
		struct RegionTranslation
		{
			region_txln_fn native_fn_ptr;
		} __packed;
		
		struct BlockTranslation
		{
			block_txln_fn native_fn_ptr;
			const IRInstruction *ir;
			uint32_t ir_count;
		} __packed;

		struct IROperand
		{
			enum IROperandType : uint8_t {
				NONE,
				CONSTANT,
				VREG,
				BLOCK,
				FUNC,
				PC
			};

			enum IRAllocationMode : uint8_t {
				NOT_ALLOCATED,
				ALLOCATED_REG,
				ALLOCATED_STACK
			};

			IROperandType type;
			uint64_t value;
			uint8_t size;

			IRAllocationMode alloc_mode;
			uint32_t alloc_data;

			IROperand() : type(NONE), value(0), size(0), alloc_mode(NOT_ALLOCATED), alloc_data(0) { }

			IROperand(IROperandType type, uint64_t value, uint8_t size) : type(type), value(value), size(size), alloc_mode(NOT_ALLOCATED), alloc_data(0) { }

			inline bool is_allocated() const { return alloc_mode != NOT_ALLOCATED; }
			inline bool is_alloc_reg() const { return alloc_mode == ALLOCATED_REG; }
			inline bool is_alloc_stack() const { return alloc_mode == ALLOCATED_STACK; }

			inline void allocate(IRAllocationMode mode, uint32_t data) { alloc_mode = mode; alloc_data = data; }

			inline bool is_valid() const { return type != NONE; }
			inline bool is_constant() const { return type == CONSTANT; }
			inline bool is_vreg() const { return type == VREG; }
			inline bool is_block() const { return type == BLOCK; }
			inline bool is_func() const { return type == FUNC; }
			inline bool is_pc() const { return type == PC; }

			static IROperand none() { return IROperand(NONE, 0, 0); }

			static IROperand const8(uint8_t value) { return IROperand(CONSTANT, value, 1); }
			static IROperand const16(uint16_t value) { return IROperand(CONSTANT, value, 2); }
			static IROperand const32(uint32_t value) { return IROperand(CONSTANT, value, 4); }
			static IROperand const64(uint64_t value) { return IROperand(CONSTANT, value, 8); }

			static IROperand vreg(IRRegId id, uint8_t size) { return IROperand(VREG, (uint64_t)id, size); }

			static IROperand pc(uint32_t offset) { return IROperand(PC, (uint64_t)offset, 4); }

			static IROperand block(IRBlockId id) { return IROperand(BLOCK, (uint64_t)id, 0); }
			static IROperand func(void *addr) { return IROperand(FUNC, (uint64_t)addr, 0); }
		} __packed;

		struct IRInstruction
		{
			enum IRInstructionType : uint8_t {
				INVALID,

				VERIFY,
				COUNT,

				NOP,
				TRAP,

				MOV,
				CMOV,
				LDPC,
				INCPC,

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
				READ_MEM_USER,
				WRITE_MEM_USER,

				CALL,
				JMP,
				BRANCH,
				RET,
				DISPATCH,

				SET_CPU_MODE,
				WRITE_DEVICE,
				READ_DEVICE,
			};

			IRBlockId ir_block;
			IRInstructionType type;
			IROperand operands[6];

			IRInstruction(IRInstructionType type)
				: type(type),
				operands { IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none() } { }

			IRInstruction(IRInstructionType type, IROperand& op1)
				: type(type),
				operands { op1, IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none() } { }

			IRInstruction(IRInstructionType type, IROperand& op1, IROperand& op2)
				: type(type),
				operands { op1, op2, IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none() } { }

			IRInstruction(IRInstructionType type, IROperand& op1, IROperand& op2, IROperand& op3)
				: type(type),
				operands { op1, op2, op3, IROperand::none(), IROperand::none(), IROperand::none() } { }

			IRInstruction(IRInstructionType type, IROperand& op1, IROperand& op2, IROperand& op3, IROperand& op4)
				: type(type),
				operands { op1, op2, op3, op4, IROperand::none(), IROperand::none() } { }

			IRInstruction(IRInstructionType type, IROperand& op1, IROperand& op2, IROperand& op3, IROperand& op4, IROperand& op5)
				: type(type),
				operands { op1, op2, op3, op4, op5, IROperand::none() } { }

			IRInstruction(IRInstructionType type, IROperand& op1, IROperand& op2, IROperand& op3, IROperand& op4, IROperand& op5, IROperand& op6)
				: type(type),
				operands { op1, op2, op3, op4, op5, op6 } { }

			static IRInstruction nop() { return IRInstruction(NOP); }
			static IRInstruction ret() { return IRInstruction(RET); }
			static IRInstruction dispatch(IROperand target, IROperand fallthrough) { assert(target.is_constant() && fallthrough.is_constant()); return IRInstruction(DISPATCH, target, fallthrough); }
			static IRInstruction trap() { return IRInstruction(TRAP); }
			static IRInstruction verify(IROperand pc) { return IRInstruction(VERIFY, pc); }
			static IRInstruction count(IROperand pc, IROperand ir) { return IRInstruction(COUNT, pc, ir); }

			static IRInstruction ldpc(IROperand dst) { assert(dst.is_vreg() && dst.size == 4); return IRInstruction(LDPC, dst); }
			static IRInstruction incpc(IROperand amt) { assert(amt.is_constant()); return IRInstruction(INCPC, amt); }

			//
			// Data Motion
			//
			static IRInstruction mov(IROperand src, IROperand dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(MOV, src, dst);
			}

			static IRInstruction cmov(IROperand cond, IROperand src, IROperand dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(CMOV, cond, src, dst);
			}

			static IRInstruction sx(IROperand src, IROperand dst) {
				assert(src.size < dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(SX, src, dst);
			}

			static IRInstruction zx(IROperand src, IROperand dst) {
				assert(src.size < dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(ZX, src, dst);
			}

			static IRInstruction trunc(IROperand src, IROperand dst) {
				assert(src.size > dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(TRUNC, src, dst);
			}

			//
			// Arithmetic Operations
			//
			static IRInstruction add(IROperand src, IROperand dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg() || src.is_pc());
				assert(dst.is_vreg());

				return IRInstruction(ADD, src, dst);
			}

			static IRInstruction sub(IROperand src, IROperand dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(SUB, src, dst);
			}

			static IRInstruction mul(IROperand src, IROperand dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(MUL, src, dst);
			}

			static IRInstruction div(IROperand src, IROperand dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(DIV, src, dst);
			}

			static IRInstruction mod(IROperand src, IROperand dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(MOD, src, dst);
			}

			// Bit-shifting
			static IRInstruction shl(IROperand amt, IROperand dst) {
				assert(amt.is_constant() || amt.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(SHL, amt, dst);
			}

			static IRInstruction shr(IROperand amt, IROperand dst) {
				assert(amt.is_constant() || amt.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(SHR, amt, dst);
			}

			static IRInstruction sar(IROperand amt, IROperand dst) {
				assert(amt.is_constant() || amt.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(SAR, amt, dst);
			}

			// Bit manipulation
			static IRInstruction clz(IROperand src, IROperand dst) {
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(CLZ, src, dst);
			}

			static IRInstruction bitwise_and(IROperand src, IROperand dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(AND, src, dst);
			}

			static IRInstruction bitwise_or(IROperand src, IROperand dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(OR, src, dst);
			}

			static IRInstruction bitwise_xor(IROperand src, IROperand dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(XOR, src, dst);
			}

			//
			// Comparison
			//
			static IRInstruction cmpeq(IROperand lh, IROperand rh, IROperand dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPEQ, lh, rh, dst);
			}

			static IRInstruction cmpne(IROperand lh, IROperand rh, IROperand dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPNE, lh, rh, dst);
			}

			static IRInstruction cmplt(IROperand lh, IROperand rh, IROperand dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPLT, lh, rh, dst);
			}

			static IRInstruction cmplte(IROperand lh, IROperand rh, IROperand dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPLTE, lh, rh, dst);
			}

			static IRInstruction cmpgt(IROperand lh, IROperand rh, IROperand dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPGT, lh, rh, dst);
			}

			static IRInstruction cmpgte(IROperand lh, IROperand rh, IROperand dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPGTE, lh, rh, dst);
			}

			//
			// Domain-specific operations
			//
			static IRInstruction ldreg(IROperand offset, IROperand dst)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(READ_REG, offset, dst);
			}

			static IRInstruction streg(IROperand src, IROperand offset)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(src.is_constant() || src.is_vreg());

				return IRInstruction(WRITE_REG, src, offset);
			}

			static IRInstruction ldmem(IROperand offset, IROperand dst)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(READ_MEM, offset, dst);
			}

			static IRInstruction stmem(IROperand src, IROperand offset)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(src.is_constant() || src.is_vreg());

				return IRInstruction(WRITE_MEM, src, offset);
			}

			static IRInstruction ldmem_user(IROperand offset, IROperand dst)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(READ_MEM_USER, offset, dst);
			}

			static IRInstruction stmem_user(IROperand src, IROperand offset)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(src.is_constant() || src.is_vreg());

				return IRInstruction(WRITE_MEM_USER, src, offset);
			}

			static IRInstruction write_device(IROperand dev, IROperand reg, IROperand val)
			{
				assert(dev.is_constant() || dev.is_vreg());
				assert(reg.is_constant() || reg.is_vreg());
				assert(val.is_constant() || val.is_vreg());

				return IRInstruction(WRITE_DEVICE, dev, reg, val);
			}

			static IRInstruction read_device(IROperand dev, IROperand reg, IROperand dst)
			{
				assert(dev.is_constant() || dev.is_vreg());
				assert(reg.is_constant() || reg.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(READ_DEVICE, dev, reg, dst);
			}

			static IRInstruction set_cpu_mode(IROperand mode)
			{
				assert(mode.is_constant() || mode.is_vreg());

				return IRInstruction(SET_CPU_MODE, mode);
			}

			//
			// Control Flow
			//
			static IRInstruction jump(IROperand target)
			{
				assert(target.is_block());
				return IRInstruction(JMP, target);
			}

			static IRInstruction branch(IROperand cond, IROperand tt, IROperand ft)
			{
				assert(cond.is_constant() || cond.is_vreg());
				assert(tt.is_block());
				assert(ft.is_block());

				return IRInstruction(BRANCH, cond, tt, ft);
			}

			static IRInstruction call(IROperand fn)
			{
				assert(fn.is_func());

				return IRInstruction(CALL, fn);
			}

			static IRInstruction call(IROperand fn, IROperand arg0)
			{
				assert(fn.is_func());
				assert(arg0.is_constant() || arg0.is_vreg());

				return IRInstruction(CALL, fn, arg0);
			}

			static IRInstruction call(IROperand fn, IROperand arg0, IROperand arg1)
			{
				assert(fn.is_func());
				assert(arg0.is_constant() || arg0.is_vreg());
				assert(arg1.is_constant() || arg1.is_vreg());

				return IRInstruction(CALL, fn, arg0, arg1);
			}

			static IRInstruction call(IROperand fn, IROperand arg0, IROperand arg1, IROperand arg2)
			{
				assert(fn.is_func());
				assert(arg0.is_constant() || arg0.is_vreg());
				assert(arg1.is_constant() || arg1.is_vreg());
				assert(arg2.is_constant() || arg2.is_vreg());

				return IRInstruction(CALL, fn, arg0, arg1, arg2);
			}

			static IRInstruction call(IROperand fn, IROperand arg0, IROperand arg1, IROperand arg2, IROperand arg3)
			{
				assert(fn.is_func());
				assert(arg0.is_constant() || arg0.is_vreg());
				assert(arg1.is_constant() || arg1.is_vreg());
				assert(arg2.is_constant() || arg2.is_vreg());
				assert(arg3.is_constant() || arg3.is_vreg());

				return IRInstruction(CALL, fn, arg0, arg1, arg2, arg3);
			}

			static IRInstruction call(IROperand fn, IROperand arg0, IROperand arg1, IROperand arg2, IROperand arg3, IROperand arg4)
			{
				assert(fn.is_func());
				assert(arg0.is_constant() || arg0.is_vreg());
				assert(arg1.is_constant() || arg1.is_vreg());
				assert(arg2.is_constant() || arg2.is_vreg());
				assert(arg3.is_constant() || arg3.is_vreg());
				assert(arg4.is_constant() || arg4.is_vreg());

				return IRInstruction(CALL, fn, arg0, arg1, arg2, arg3, arg4);
			}
		} __packed;
	}
}

#endif	/* SHARED_JIT_H */

