/*
 * File:   block-compiler.h
 * Author: spink
 *
 * Created on 27 April 2015, 11:58
 */

#ifndef BLOCK_COMPILER_H
#define	BLOCK_COMPILER_H

#include <define.h>
#include <shared-jit.h>
#include <jit/ir.h>
#include <x86/encode.h>
#include <local-memory.h>

#include <map>
#include <vector>
#include <set>

namespace captive {
	namespace arch {
		namespace jit {
			typedef uint32_t (*block_txln_fn)(void *);

			class BlockCompiler
			{
			public:
				BlockCompiler(shared::TranslationBlock& tb);
				bool compile(block_txln_fn& fn);

			private:
				LocalMemory memory_allocator;
				shared::TranslationBlock& tb;
				IRContext ir;
				x86::X86Encoder encoder;

				std::map<uint32_t, shared::IRBlockId> block_relocations;
				std::map<uint64_t, x86::X86Register *> register_assignments_1;
				std::map<uint64_t, x86::X86Register *> register_assignments_2;
				std::map<uint64_t, x86::X86Register *> register_assignments_4;
				std::map<uint64_t, x86::X86Register *> register_assignments_8;

				inline void assign(uint8_t id, x86::X86Register& r8, x86::X86Register& r4, x86::X86Register& r2, x86::X86Register& r1)
				{
					register_assignments_8[id] = &r8;
					register_assignments_4[id] = &r4;
					register_assignments_2[id] = &r2;
					register_assignments_1[id] = &r1;
				}

				bool optimise_tb();
				bool build();
				bool analyse();
				bool optimise_ir();

				bool allocate(uint32_t& max_stack);

				bool lower(uint32_t max_stack, block_txln_fn& fn);
				bool lower_block(IRBlock& block);

				IRInstruction *instruction_from_shared(IRContext& ctx, const shared::IRInstruction *insn);
				IROperand *operand_from_shared(IRContext& ctx, const shared::IROperand *operand);

				void load_state_field(uint32_t slot, x86::X86Register& reg);
				void encode_operand_to_reg(IROperand& operand, x86::X86Register& reg);

				inline x86::X86Register& register_from_operand(IRRegisterOperand& oper, int force_width = 0) const
				{
					assert(oper.is_allocated_reg());

					if (!force_width) force_width = oper.reg().width();

					switch (force_width) {
					case 1:	return *(register_assignments_1.find(oper.allocation_data())->second);
					case 2:	return *(register_assignments_2.find(oper.allocation_data())->second);
					case 4:	return *(register_assignments_4.find(oper.allocation_data())->second);
					case 8:	return *(register_assignments_8.find(oper.allocation_data())->second);
					default: assert(false);
					}
				}

				inline x86::X86Memory stack_from_operand(IRRegisterOperand& oper) const
				{
					assert(oper.is_allocated_stack());
					assert(oper.reg().width() < 8);

					return x86::X86Memory(x86::REG_RBP, (oper.allocation_data() * -4) - 4);
				}

				inline void unspill(IRRegisterOperand& oper, x86::X86Register& reg)
				{
					encoder.mov(stack_from_operand(oper), reg);
				}

				void emit_save_reg_state();
				void emit_restore_reg_state();

				bool merge_blocks();
				bool thread_jumps();
				bool thread_rets();
				bool dse();
				bool die();
			};
		}
	}
}

#endif	/* BLOCK_COMPILER_H */

