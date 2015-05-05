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
				std::map<uint64_t, x86::X86Register *> register_assignments;

				bool optimise_tb();
				bool build();
				bool optimise_ir();

				bool allocate(uint32_t& max_stack);

				bool lower(uint32_t max_stack, block_txln_fn& fn);
				bool lower_block(IRBlock& block);

				IRInstruction *instruction_from_shared(IRContext& ctx, const shared::IRInstruction *insn);
				IROperand *operand_from_shared(IRContext& ctx, const shared::IROperand *operand);

				void load_state_field(uint32_t slot, x86::X86Register& reg);
				void encode_operand_to_reg(IROperand& operand, x86::X86Register& reg);

				inline x86::X86Register& register_from_operand(IRRegisterOperand& oper) const
				{
					assert(oper.is_allocated_reg());
					return *(register_assignments.find(oper.allocation_data())->second);
				}

				inline x86::X86Memory stack_from_operand(IRRegisterOperand& oper) const
				{
					assert(oper.is_allocated_stack());
					return x86::X86Memory(x86::REG_RBP, (oper.allocation_data() * -1) - 4);
				}
			};
		}
	}
}

#endif	/* BLOCK_COMPILER_H */

