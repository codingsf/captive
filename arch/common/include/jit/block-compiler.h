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
#include <local-memory.h>
#include <jit/translation-context.h>

#include <small-set.h>
#include <map>
#include <vector>

#define REGSTATE_REG REG_RDI
#define JITSTATE_REG REG_R14

namespace captive {
	namespace arch {
		namespace jit {
			class BlockCompiler
			{
			public:
				BlockCompiler(TranslationContext& ctx, gpa_t pa, bool emit_interrupt_check = false);
				bool compile(shared::block_txln_fn& fn);

			private:
				TranslationContext& ctx;
				x86::X86Encoder encoder;
				gpa_t pa;
				bool ichk;

				PopulatedSet<8> used_phys_regs;

				typedef std::map<shared::IRBlockId, std::vector<shared::IRBlockId>> cfg_t;
				typedef std::vector<shared::IRBlockId> block_list_t;

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
				bool peeplower(uint32_t max_stack);

				void dump_ir();

				void emit_save_reg_state(int num_operands);
				void emit_restore_reg_state(int num_operands);
				void encode_operand_function_argument(shared::IROperand *oper, const x86::X86Register& reg);
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
					assert(false);
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
					default: assert(false);
					}
				}

				inline x86::X86Memory stack_from_operand(const captive::shared::IROperand *oper) const
				{
					assert(oper->alloc_mode == captive::shared::IROperand::ALLOCATED_STACK);
					assert(oper->size < 8);

					return x86::X86Memory(x86::REG_RBP, (oper->alloc_data * -1) - 8);
				}

				inline x86::X86Register& get_temp(int id, int width)
				{
					switch (id) {
					case 0:
						switch (width) {
						case 1:	return x86::REG_BL;
						case 2:	return x86::REG_BX;
						case 4:	return x86::REG_EBX;
						case 8:	return x86::REG_RBX;
						default: assert(false);
						}

					case 1:
						switch (width) {
						case 1:	return x86::REG_CL;
						case 2:	return x86::REG_CX;
						case 4:	return x86::REG_ECX;
						case 8:	return x86::REG_RCX;
						default: assert(false);
						}

					default: assert(false);
					}
				}

				inline x86::X86Register& unspill_temp(const captive::shared::IROperand *oper, int id)
				{
					x86::X86Register& tmp = get_temp(id, oper->size);
					encoder.mov(stack_from_operand(oper), tmp);
					return tmp;
				}

				inline void load_state_field(uint8_t slot, const x86::X86Register& reg)
				{
					encoder.mov(x86::X86Memory::get(x86::JITSTATE_REG, slot), reg);
				}
			};
		}
	}
}

#endif	/* BLOCK_COMPILER_H */

