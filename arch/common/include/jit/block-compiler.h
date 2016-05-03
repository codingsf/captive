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
#include <x86/encode.h>
#include <jit/translation-context.h>
#include <malloc/malloc.h>
#include <cpu.h>

#include <small-set.h>
#include <map>
#include <vector>

#define REGSTATE_REG		REG_RBP
//#define CALLER_SAVE_REGSTATE

#define FRAMEPOINTER_REG	REG_R13

namespace captive {
	namespace arch {
		namespace jit {
			class BlockCompiler
			{
			public:
				BlockCompiler(TranslationContext& ctx, malloc::Allocator& allocator, uint8_t isa_mode, gpa_t pa, const CPU::TaggedRegisters& tagged_regs);
				bool compile(shared::block_txln_fn& fn);

			private:
				TranslationContext& ctx;
				x86::X86Encoder encoder;
				uint8_t isa_mode;
				gpa_t pa;
				const CPU::TaggedRegisters& tagged_regs;
				
				PopulatedSet<9> used_phys_regs;

				typedef std::map<shared::IRBlockId, std::vector<shared::IRBlockId>> cfg_t;
				typedef std::vector<shared::IRBlockId> block_list_t;

				bool verify();
				bool sort_ir();
				bool peephole();
				bool analyse(uint32_t& max_stack);
				bool dbe();
				bool merge_blocks();
				bool merge_block(shared::IRBlockId mergee, shared::IRBlockId merger);
				bool thread_jumps();
				bool build_cfg(block_list_t& blocks, cfg_t& succs, cfg_t& preds, block_list_t& exits);
				bool allocate();
				bool post_allocate_peephole();
				bool lower(uint32_t max_stack);
				bool lower_to_interpreter();
				bool peeplower(uint32_t max_stack);
				bool lower_stack_to_reg();
				bool constant_prop();
				bool reorder_blocks();
				bool reg_value_reuse();
				bool value_merging();

				void dump_ir();

				typedef std::map<const x86::X86Register*, uint32_t> stack_map_t;
				void emit_save_reg_state(int num_operands, stack_map_t&);
				void emit_restore_reg_state(int num_operands, stack_map_t&);
				void encode_operand_function_argument(shared::IROperand *oper, const x86::X86Register& reg, stack_map_t&);
				void encode_operand_to_reg(shared::IROperand *operand, const x86::X86Register& reg);

				std::map<uint64_t, const x86::X86Register *> register_assignments_1;
				std::map<uint64_t, const x86::X86Register *> register_assignments_2;
				std::map<uint64_t, const x86::X86Register *> register_assignments_4;
				std::map<uint64_t, const x86::X86Register *> register_assignments_8;

				inline const x86::X86Register &get_allocable_register(int index, int size)
				{
					switch(size) {
						case 1: return *register_assignments_1[index];
						case 2: return *register_assignments_2[index];
						case 4: return *register_assignments_4[index];
						case 8: return *register_assignments_8[index];
					}
					
					should_not_reach();
				}

				inline void assign(uint8_t id, const x86::X86Register& r8, const x86::X86Register& r4, const x86::X86Register& r2, const x86::X86Register& r1)
				{
					register_assignments_8[id] = &r8;
					register_assignments_4[id] = &r4;
					register_assignments_2[id] = &r2;
					register_assignments_1[id] = &r1;
				}

				inline const x86::X86Register& register_from_operand(const captive::shared::IROperand *oper, int force_width = 0) const
				{
					assert(oper->alloc_mode == captive::shared::IROperand::ALLOCATED_REG);

					if (!force_width) force_width = oper->size;

					switch (force_width) {
					case 1:	return *(register_assignments_1.find(oper->alloc_data)->second);
					case 2:	return *(register_assignments_2.find(oper->alloc_data)->second);
					case 4:	return *(register_assignments_4.find(oper->alloc_data)->second);
					case 8:	return *(register_assignments_8.find(oper->alloc_data)->second);
					}
					
					should_not_reach();
				}

				inline x86::X86Memory stack_from_operand(const captive::shared::IROperand *oper) const
				{
					assert(oper->alloc_mode == captive::shared::IROperand::ALLOCATED_STACK);
					assert(oper->size < 8);

					return x86::X86Memory(x86::FRAMEPOINTER_REG, (oper->alloc_data * -1) - 8);
				}

				inline x86::X86Register& get_temp(int id, int width)
				{
					switch (id) {
					case 0:
						switch (width) {
						case 1:	return x86::REG_CL;
						case 2:	return x86::REG_CX;
						case 4:	return x86::REG_ECX;
						case 8:	return x86::REG_RCX;
						}

					case 1:
						switch (width) {
						case 1:	return x86::REG_R14B;
						case 2:	return x86::REG_R14W;
						case 4:	return x86::REG_R14D;
						case 8:	return x86::REG_R14;
						}
					}
					
					should_not_reach();
				}

				inline x86::X86Register& unspill_temp(const captive::shared::IROperand *oper, int id)
				{
					x86::X86Register& tmp = get_temp(id, oper->size);
					encoder.mov(stack_from_operand(oper), tmp);
					return tmp;
				}

				inline void load_state_field(uint8_t slot, const x86::X86Register& reg)
				{
					encoder.mov(x86::X86Memory::get(x86::REG_FS, slot), reg);
				}
			};
		}
	}
}

#endif	/* BLOCK_COMPILER_H */

