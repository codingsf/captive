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

			union {
				uint64_t value;
				float fvalue;
				double dvalue;
			};
			
			uint16_t alloc_data : 14;
			IRAllocationMode alloc_mode : 2;
			IROperandType type : 4;
			uint8_t size : 4;

			IROperand() : value(0), alloc_data(0), alloc_mode(NOT_ALLOCATED), type(NONE), size(0) { }

			IROperand(IROperandType type, uint64_t value, uint8_t size) : value(value), alloc_data(0), alloc_mode(NOT_ALLOCATED), type(type), size(size) { }
			IROperand(IROperandType type, float value) : fvalue(value), alloc_data(0), alloc_mode(NOT_ALLOCATED), type(type), size(sizeof(float)) { }
			IROperand(IROperandType type, double value) : dvalue(value), alloc_data(0), alloc_mode(NOT_ALLOCATED), type(type), size(sizeof(double)) { }

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
			static IROperand constFloat(float value) { return IROperand(CONSTANT, value); }
			static IROperand constDouble(double value) { return IROperand(CONSTANT, value); }

			static IROperand vreg(IRRegId id, uint8_t size) { return IROperand(VREG, (uint64_t)id, size); }

			static IROperand pc(uint32_t offset) { return IROperand(PC, (uint64_t)offset, 4); }

			static IROperand block(IRBlockId id) { return IROperand(BLOCK, (uint64_t)id, 0); }
			static IROperand func(void *addr) { return IROperand(FUNC, (uint64_t)addr, 0); }
			
			bool operator<(const IROperand& other) const
			{
				return alloc_data < other.alloc_data;
			}
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
				STPC,
				INCPC,
				
				VECTOR_INSERT,
				VECTOR_EXTRACT,

				ADD,					// 9
				ADC,
				SUB,
				SBC,
				MUL,
				DIV,
				MOD,
				
				ABS,
				NEG,
				SQRT,
				IS_QNAN,
				IS_SNAN,

				SHL,					// 16
				SHR,
				SAR,
				ROR,
				CLZ,

				NOT,
				AND,					// 21
				OR,
				XOR,

				CMPEQ,					// 24
				CMPNE,
				CMPGT,
				CMPGTE,
				CMPLT,
				CMPLTE,

				SX,						// 30
				ZX,
				TRUNC,

				READ_REG,				// 33
				WRITE_REG,
				READ_MEM,
				WRITE_MEM,
				READ_MEM_USER,
				WRITE_MEM_USER,
				
				ATOMIC_WRITE,

				CALL,					// 40
				JMP,
				BRANCH,
				RET,
				LOOP,
				DISPATCH,

				SET_CPU_MODE,			// 45
				WRITE_DEVICE,
				READ_DEVICE,
				
				TRIGGER_IRQ,			// 48
				
				FLUSH,					// 48
				FLUSH_ITLB,
				FLUSH_DTLB,
				FLUSH_ITLB_ENTRY,
				FLUSH_DTLB_ENTRY,
				FLUSH_CONTEXT_ID,
				INVALIDATE_ICACHE,
				INVALIDATE_ICACHE_ENTRY,
				SET_CONTEXT_ID,
				PGT_CHANGE,
				
				SWITCH_TO_KERNEL_MODE,
				SWITCH_TO_USER_MODE,
				
				ADC_WITH_FLAGS,			// 53
				SBC_WITH_FLAGS,
				SET_ZN_FLAGS,
				
				BARRIER,				// 56
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

			IRInstruction *unsafe_next() const { return (IRInstruction *)(((uint64_t)this) + sizeof(IRInstruction)); }
			
			static IRInstruction nop() { return IRInstruction(NOP); }
			static IRInstruction ret() { return IRInstruction(RET); }
			static IRInstruction loop() { return IRInstruction(LOOP); }
			static IRInstruction dispatch(const IROperand& target, const IROperand& fallthrough, const IROperand& target_block, const IROperand& fallthrough_block) { assert(target.is_constant() && fallthrough.is_constant()); return IRInstruction(DISPATCH, target, fallthrough, target_block, fallthrough_block); }
			static IRInstruction trap() { return IRInstruction(TRAP); }
			static IRInstruction verify(const IROperand& pc) { return IRInstruction(VERIFY, pc); }
			static IRInstruction count(const IROperand& pc, const IROperand& ir) { return IRInstruction(COUNT, pc, ir); }

			static IRInstruction ldpc(const IROperand& dst) { assert(dst.is_vreg() && dst.size == 4); return IRInstruction(LDPC, dst); }
			static IRInstruction stpc(const IROperand& val) { assert(val.size == 4); return IRInstruction(STPC, val); }
			static IRInstruction incpc(const IROperand& amt) { assert(amt.is_constant()); return IRInstruction(INCPC, amt); }

			static IRInstruction trigger_irq() { return IRInstruction(TRIGGER_IRQ); }
			
			static IRInstruction flush() { return IRInstruction(FLUSH); }
			static IRInstruction flush_itlb() { return IRInstruction(FLUSH_ITLB); }
			static IRInstruction flush_dtlb() { return IRInstruction(FLUSH_DTLB); }
			static IRInstruction flush_itlb_entry(const IROperand& addr) { return IRInstruction(FLUSH_ITLB_ENTRY, addr); }
			static IRInstruction flush_dtlb_entry(const IROperand& addr) { return IRInstruction(FLUSH_DTLB_ENTRY, addr); }
			static IRInstruction flush_context_id(const IROperand& ctxid) { return IRInstruction(FLUSH_CONTEXT_ID, ctxid); }
			
			static IRInstruction invalidate_icache() { return IRInstruction(INVALIDATE_ICACHE); }
			static IRInstruction invalidate_icache_entry(const IROperand& addr) { return IRInstruction(INVALIDATE_ICACHE_ENTRY, addr); }
			
			static IRInstruction set_context_id(const IROperand& ctxid) { return IRInstruction(SET_CONTEXT_ID, ctxid); }
			static IRInstruction pgt_change() { return IRInstruction(PGT_CHANGE); }

			static IRInstruction switch_to_kernel_mode() { return IRInstruction(SWITCH_TO_KERNEL_MODE); }
			static IRInstruction switch_to_user_mode() { return IRInstruction(SWITCH_TO_USER_MODE); }
			
			static IRInstruction barrier(const IROperand& pc, const IROperand& ir) { assert(pc.is_pc()); return IRInstruction(BARRIER, pc, ir); }
			
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
			// Flag Manipulation
			//
			static IRInstruction set_zn_flags(const IROperand& val) { return IRInstruction(SET_ZN_FLAGS, val); }
			
			//
			// Data Motion
			//
			static IRInstruction mov(const IROperand& src, const IROperand& dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(MOV, src, dst);
			}

			static IRInstruction cmov(const IROperand& cond, const IROperand& src, const IROperand& dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(CMOV, cond, src, dst);
			}

			static IRInstruction sx(const IROperand& src, const IROperand& dst) {
				assert(src.size < dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(SX, src, dst);
			}

			static IRInstruction zx(const IROperand& src, const IROperand& dst) {
				assert(src.size < dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(ZX, src, dst);
			}

			static IRInstruction trunc(const IROperand& src, const IROperand& dst) {
				assert(src.size > dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(TRUNC, src, dst);
			}
			
			static IRInstruction vector_insert(const IROperand& vector, const IROperand& index, const IROperand& src) {
				assert(vector.is_constant() || vector.is_vreg());
				assert(index.is_constant() || index.is_vreg());
				assert(src.is_constant() || src.is_vreg());
				
				return IRInstruction(VECTOR_INSERT, vector, index, src);
			}
			
			static IRInstruction vector_extract(const IROperand& vector, const IROperand& index, const IROperand& dst) {
				assert(vector.is_constant() || vector.is_vreg());
				assert(index.is_constant() || index.is_vreg());
				assert(dst.is_vreg());
				
				return IRInstruction(VECTOR_EXTRACT, vector, index, dst);
			}

			//
			// Arithmetic Operations
			//
			static IRInstruction add(const IROperand& src, const IROperand& dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg() || src.is_pc());
				assert(dst.is_vreg());

				return IRInstruction(ADD, src, dst);
			}

			static IRInstruction sub(const IROperand& src, const IROperand& dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(SUB, src, dst);
			}

			static IRInstruction mul(const IROperand& src, const IROperand& dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(MUL, src, dst);
			}

			static IRInstruction div(const IROperand& src, const IROperand& dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(DIV, src, dst);
			}

			static IRInstruction mod(const IROperand& src, const IROperand& dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(MOD, src, dst);
			}
			
			static IRInstruction adc(const IROperand& src, const IROperand& dst, const IROperand& carry) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());
				assert(carry.is_constant() || carry.is_vreg());
				
				return IRInstruction(ADC, src, dst, carry);
			}
			
			static IRInstruction adc_with_flags(const IROperand& src, const IROperand& dst, const IROperand& carry) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());
				assert(carry.is_constant() || carry.is_vreg());
				
				return IRInstruction(ADC_WITH_FLAGS, src, dst, carry);
			}
			
			static IRInstruction sbc(const IROperand& src, const IROperand& dst, const IROperand& carry) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());
				assert(carry.is_constant() || carry.is_vreg());
				
				return IRInstruction(SBC, src, dst, carry);
			}
			
			static IRInstruction sbc_with_flags(const IROperand& src, const IROperand& dst, const IROperand& carry) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());
				assert(carry.is_constant() || carry.is_vreg());
				
				return IRInstruction(SBC_WITH_FLAGS, src, dst, carry);
			}
			
			static IRInstruction abs(const IROperand& val)
			{
				assert(val.is_vreg());
				return IRInstruction(ABS, val);
			}
			
			static IRInstruction neg(const IROperand& val)
			{
				assert(val.is_vreg());
				return IRInstruction(NEG, val);
			}
			
			static IRInstruction sqrt(const IROperand& val)
			{
				assert(val.is_vreg());
				return IRInstruction(SQRT, val);
			}
			
			static IRInstruction is_qnan(const IROperand& src, const IROperand& dst)
			{
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());
				return IRInstruction(IS_QNAN, src, dst);
			}
			
			static IRInstruction is_snan(const IROperand& src, const IROperand& dst)
			{
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());
				return IRInstruction(IS_SNAN, src, dst);
			}
			
			// Bit-shifting
			static IRInstruction shl(const IROperand& amt, const IROperand& dst) {
				assert(amt.is_constant() || amt.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(SHL, amt, dst);
			}

			static IRInstruction shr(const IROperand& amt, const IROperand& dst) {
				assert(amt.is_constant() || amt.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(SHR, amt, dst);
			}

			static IRInstruction sar(const IROperand& amt, const IROperand& dst) {
				assert(amt.is_constant() || amt.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(SAR, amt, dst);
			}
			
			static IRInstruction ror(const IROperand& amt, const IROperand& dst) {
				assert(amt.is_constant() || amt.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(ROR, amt, dst);
			}

			// Bit manipulation
			static IRInstruction clz(const IROperand& src, const IROperand& dst) {
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(CLZ, src, dst);
			}

			static IRInstruction bitwise_not(const IROperand& dst) {
				assert(dst.is_vreg());

				return IRInstruction(NOT, dst);
			}
			
			static IRInstruction bitwise_and(const IROperand& src, const IROperand& dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(AND, src, dst);
			}

			static IRInstruction bitwise_or(const IROperand& src, const IROperand& dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(OR, src, dst);
			}

			static IRInstruction bitwise_xor(const IROperand& src, const IROperand& dst) {
				assert(src.size == dst.size);
				assert(src.is_constant() || src.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(XOR, src, dst);
			}

			//
			// Comparison
			//
			static IRInstruction cmpeq(const IROperand& lh, const IROperand& rh, const IROperand& dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPEQ, lh, rh, dst);
			}

			static IRInstruction cmpne(const IROperand& lh, const IROperand& rh, const IROperand& dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPNE, lh, rh, dst);
			}

			static IRInstruction cmplt(const IROperand& lh, const IROperand& rh, const IROperand& dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPLT, lh, rh, dst);
			}

			static IRInstruction cmplte(const IROperand& lh, const IROperand& rh, const IROperand& dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPLTE, lh, rh, dst);
			}

			static IRInstruction cmpgt(const IROperand& lh, const IROperand& rh, const IROperand& dst)
			{
				assert(lh.size == rh.size);
				assert(lh.is_vreg() || lh.is_constant());
				assert(rh.is_vreg() || rh.is_constant());
				assert(dst.is_vreg());

				return IRInstruction(CMPGT, lh, rh, dst);
			}

			static IRInstruction cmpgte(const IROperand& lh, const IROperand& rh, const IROperand& dst)
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
			static IRInstruction ldreg(const IROperand& offset, const IROperand& dst)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(READ_REG, offset, dst);
			}

			static IRInstruction streg(const IROperand& src, const IROperand& offset)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(src.is_constant() || src.is_vreg());

				return IRInstruction(WRITE_REG, src, offset);
			}

			static IRInstruction ldmem(const IROperand& offset, const IROperand& dst)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(READ_MEM, offset, IROperand::const32(0), dst);
			}

			static IRInstruction stmem(const IROperand& src, const IROperand& offset)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(src.is_constant() || src.is_vreg());

				return IRInstruction(WRITE_MEM, src, IROperand::const32(0), offset);
			}

			static IRInstruction ldmem_user(const IROperand& offset, const IROperand& dst)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(READ_MEM_USER, offset, dst);
			}

			static IRInstruction stmem_user(const IROperand& src, const IROperand& offset)
			{
				assert(offset.is_constant() || offset.is_vreg());
				assert(src.is_constant() || src.is_vreg());

				return IRInstruction(WRITE_MEM_USER, src, offset);
			}
			
			static IRInstruction atomic_write(const IROperand& addr, const IROperand& val)
			{
				assert(addr.is_constant() || addr.is_vreg());
				assert(val.is_vreg());

				return IRInstruction(ATOMIC_WRITE, addr, val);
			}

			static IRInstruction write_device(const IROperand& dev, const IROperand& reg, const IROperand& val)
			{
				assert(dev.is_constant() || dev.is_vreg());
				assert(reg.is_constant() || reg.is_vreg());
				assert(val.is_constant() || val.is_vreg());

				return IRInstruction(WRITE_DEVICE, dev, reg, val);
			}

			static IRInstruction read_device(const IROperand& dev, const IROperand& reg, const IROperand& dst)
			{
				assert(dev.is_constant() || dev.is_vreg());
				assert(reg.is_constant() || reg.is_vreg());
				assert(dst.is_vreg());

				return IRInstruction(READ_DEVICE, dev, reg, dst);
			}

			static IRInstruction set_cpu_mode(const IROperand& mode)
			{
				assert(mode.is_constant() || mode.is_vreg());

				return IRInstruction(SET_CPU_MODE, mode);
			}

			//
			// Control Flow
			//
			static IRInstruction jump(const IROperand& target)
			{
				assert(target.is_block());
				return IRInstruction(JMP, target);
			}

			static IRInstruction branch(const IROperand& cond, const IROperand& tt, const IROperand& ft)
			{
				assert(cond.is_constant() || cond.is_vreg());
				assert(tt.is_block());
				assert(ft.is_block());

				return IRInstruction(BRANCH, cond, tt, ft);
			}

			static IRInstruction call(const IROperand& fn)
			{
				assert(fn.is_func());

				return IRInstruction(CALL, fn);
			}

			static IRInstruction call(const IROperand& fn, const IROperand& arg0)
			{
				assert(fn.is_func());
				assert(arg0.is_constant() || arg0.is_vreg());

				return IRInstruction(CALL, fn, arg0);
			}

			static IRInstruction call(const IROperand& fn, const IROperand& arg0, const IROperand& arg1)
			{
				assert(fn.is_func());
				assert(arg0.is_constant() || arg0.is_vreg());
				assert(arg1.is_constant() || arg1.is_vreg());

				return IRInstruction(CALL, fn, arg0, arg1);
			}

			static IRInstruction call(const IROperand& fn, const IROperand& arg0, const IROperand& arg1, const IROperand& arg2)
			{
				assert(fn.is_func());
				assert(arg0.is_constant() || arg0.is_vreg());
				assert(arg1.is_constant() || arg1.is_vreg());
				assert(arg2.is_constant() || arg2.is_vreg());

				return IRInstruction(CALL, fn, arg0, arg1, arg2);
			}

			static IRInstruction call(const IROperand& fn, const IROperand& arg0, const IROperand& arg1, const IROperand& arg2, const IROperand& arg3)
			{
				assert(fn.is_func());
				assert(arg0.is_constant() || arg0.is_vreg());
				assert(arg1.is_constant() || arg1.is_vreg());
				assert(arg2.is_constant() || arg2.is_vreg());
				assert(arg3.is_constant() || arg3.is_vreg());

				return IRInstruction(CALL, fn, arg0, arg1, arg2, arg3);
			}

			static IRInstruction call(const IROperand& fn, const IROperand& arg0, const IROperand& arg1, const IROperand& arg2, const IROperand& arg3, const IROperand& arg4)
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

