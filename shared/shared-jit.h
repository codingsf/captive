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
		
#define INVALID_BLOCK_ID ((IRBlockId)-1)
		
		struct IRInstruction;
		
		typedef uint32_t (*block_txln_fn)(void *);
		typedef uint32_t (*region_txln_fn)(void *);
		
		struct BlockWorkUnit
		{
			uint32_t offset;
			const IRInstruction *ir;
			unsigned int ir_count;
			bool interrupt_check;
			bool entry_block;
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
			enum IROperandType {
				NONE,
				CONSTANT,
				VREG,
				BLOCK,
				FUNC,
				PC
			};

			enum IRAllocationMode {
				NOT_ALLOCATED,
				ALLOCATED_REG,
				ALLOCATED_STACK
			};

			uint64_t value;
			uint16_t alloc_data : 14;
			IRAllocationMode alloc_mode : 2;
			IROperandType type : 4;
			uint8_t size : 4;

			IROperand() : value(0), alloc_data(0), alloc_mode(NOT_ALLOCATED), type(NONE), size(0) { }

			IROperand(IROperandType type, uint64_t value, uint8_t size) : value(value), alloc_data(0), alloc_mode(NOT_ALLOCATED), type(type), size(size) { }

			inline bool is_allocated() const { return alloc_mode != NOT_ALLOCATED; }
			inline bool is_alloc_reg() const { return alloc_mode == ALLOCATED_REG; }
			inline bool is_alloc_stack() const { return alloc_mode == ALLOCATED_STACK; }

			inline void allocate(IRAllocationMode mode, uint16_t data) { alloc_mode = mode; alloc_data = data; }

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
				INVALID,				// 0

				VERIFY,					// 1
				COUNT,

				NOP,					// 3
				TRAP,

				MOV,					// 5
				CMOV,
				LDPC,
				INCPC,

				ADD,					// 9
				SUB,
				MUL,
				DIV,
				MOD,

				SHL,					// 14
				SHR,
				SAR,
				CLZ,

				AND,					// 18
				OR,
				XOR,

				CMPEQ,					// 21
				CMPNE,
				CMPGT,
				CMPGTE,
				CMPLT,
				CMPLTE,

				SX,						// 27
				ZX,
				TRUNC,

				READ_REG,				// 30
				WRITE_REG,
				READ_MEM,
				WRITE_MEM,
				READ_MEM_USER,
				WRITE_MEM_USER,

				CALL,					// 36
				JMP,
				BRANCH,
				RET,
				DISPATCH,

				SET_CPU_MODE,			// 41
				WRITE_DEVICE,
				READ_DEVICE,
				
				FLUSH,					// 44
				FLUSH_ITLB,
				FLUSH_DTLB,
				FLUSH_ITLB_ENTRY,
				FLUSH_DTLB_ENTRY,
				
				ADC_WITH_FLAGS,			// 49
				
				BARRIER,				// 50
				TRACE
			};

			IRBlockId ir_block;
			IRInstructionType type;
			IROperand operands[6];

			IRInstruction(IRInstructionType type)
				: type(type),
				operands { IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none() } { }

			IRInstruction(IRInstructionType type, const IROperand& op1)
				: type(type),
				operands { op1, IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none() } { }

			IRInstruction(IRInstructionType type, const IROperand& op1, const IROperand& op2)
				: type(type),
				operands { op1, op2, IROperand::none(), IROperand::none(), IROperand::none(), IROperand::none() } { }

			IRInstruction(IRInstructionType type, const IROperand& op1, const IROperand& op2, const IROperand& op3)
				: type(type),
				operands { op1, op2, op3, IROperand::none(), IROperand::none(), IROperand::none() } { }

			IRInstruction(IRInstructionType type, const IROperand& op1, const IROperand& op2, const IROperand& op3, const IROperand& op4)
				: type(type),
				operands { op1, op2, op3, op4, IROperand::none(), IROperand::none() } { }

			IRInstruction(IRInstructionType type, const IROperand& op1, const IROperand& op2, const IROperand& op3, const IROperand& op4, const IROperand& op5)
				: type(type),
				operands { op1, op2, op3, op4, op5, IROperand::none() } { }

			IRInstruction(IRInstructionType type, const IROperand& op1, const IROperand& op2, const IROperand& op3, const IROperand& op4, const IROperand& op5, const IROperand& op6)
				: type(type),
				operands { op1, op2, op3, op4, op5, op6 } { }

			uint8_t count_operands() const { int count = 0; for(int i = 0; i < 6; ++i) count += operands[i].type == IROperand::NONE; return count; }

			static IRInstruction nop() { return IRInstruction(NOP); }
			static IRInstruction ret() { return IRInstruction(RET); }
			static IRInstruction dispatch(const IROperand& target, const IROperand& fallthrough, const IROperand& target_block, const IROperand& fallthrough_block) { assert(target.is_constant() && fallthrough.is_constant()); return IRInstruction(DISPATCH, target, fallthrough, target_block, fallthrough_block); }
			static IRInstruction trap() { return IRInstruction(TRAP); }
			static IRInstruction verify(const IROperand& pc) { return IRInstruction(VERIFY, pc); }
			static IRInstruction count(const IROperand& pc, const IROperand& ir) { return IRInstruction(COUNT, pc, ir); }

			static IRInstruction ldpc(const IROperand& dst) { assert(dst.is_vreg() && dst.size == 4); return IRInstruction(LDPC, dst); }
			static IRInstruction incpc(const IROperand& amt) { assert(amt.is_constant()); return IRInstruction(INCPC, amt); }

			static IRInstruction flush() { return IRInstruction(FLUSH); }
			static IRInstruction flush_itlb() { return IRInstruction(FLUSH_ITLB); }
			static IRInstruction flush_dtlb() { return IRInstruction(FLUSH_DTLB); }
			static IRInstruction flush_itlb_entry(const IROperand& addr) { return IRInstruction(FLUSH_ITLB_ENTRY, addr); }
			static IRInstruction flush_dtlb_entry(const IROperand& addr) { return IRInstruction(FLUSH_DTLB_ENTRY, addr); }
			
			static IRInstruction adc_with_flags(const IROperand& lhs_in, const IROperand& rhs_in, const IROperand& carry_in) { return IRInstruction(ADC_WITH_FLAGS, lhs_in, rhs_in, carry_in); }
			
			static IRInstruction barrier() { return IRInstruction(BARRIER); }
			
			static IRInstruction trace_start() { return IRInstruction(TRACE, IROperand::const8(0)); }
			static IRInstruction trace_end() { return IRInstruction(TRACE, IROperand::const8(1)); }
			static IRInstruction trace_mem_read(const IROperand& addr, const IROperand& val, int width) { return IRInstruction(TRACE, IROperand::const8(2), addr, val, IROperand::const8(width)); }
			static IRInstruction trace_mem_write(const IROperand& addr, const IROperand& val, int width) { return IRInstruction(TRACE, IROperand::const8(3), addr, val, IROperand::const8(width)); }
			static IRInstruction trace_reg_read(const IROperand& reg, const IROperand& val) { return IRInstruction(TRACE, IROperand::const8(4), reg, val); }
			static IRInstruction trace_reg_write(const IROperand& reg, const IROperand& val) { return IRInstruction(TRACE, IROperand::const8(5), reg, val); }
			static IRInstruction trace_dev_read(const IROperand& dev, const IROperand& reg, const IROperand& val) { return IRInstruction(TRACE, IROperand::const8(6), dev, reg, val); }
			static IRInstruction trace_dev_write(const IROperand& dev, const IROperand& reg, const IROperand& val) { return IRInstruction(TRACE, IROperand::const8(7), dev, reg, val); }
			static IRInstruction trace_not_taken() { return IRInstruction(TRACE, IROperand::const8(8)); }
			
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

				return IRInstruction(READ_MEM, offset, IROperand::const32(0), dst);
			}

			static IRInstruction stmem(IROperand src, IROperand offset)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(src.is_constant() || src.is_vreg());

				return IRInstruction(WRITE_MEM, src, IROperand::const32(0), offset);
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

