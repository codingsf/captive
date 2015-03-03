/*
 * File:   translation-context.h
 * Author: spink
 *
 * Created on 27 February 2015, 15:07
 */

#ifndef TRANSLATION_CONTEXT_H
#define	TRANSLATION_CONTEXT_H

#include <define.h>
#include <malloc.h>
#include <jit/guest-basic-block.h>
#include <jit/ir-instruction.h>

namespace captive {
	namespace arch {
		namespace jit {
			class TranslationContext
			{
			public:
				TranslationContext(void *instruction_buffer);

				GuestBasicBlock::GuestBasicBlockFn compile();

				inline void add_instruction(const IRInstruction& instruction) {
					add_instruction(current_block_id, instruction);
				}

				inline void add_instruction(block_id_t block_id, const IRInstruction& instruction) {
					instruction_buffer->entries[instruction_buffer->entry_count].block_id = block_id;
					instruction_buffer->entries[instruction_buffer->entry_count].instruction = instruction;

					for (int i = 0; i < 4; i++) {
						IROperand *oper = &instruction_buffer->entries[instruction_buffer->entry_count].instruction.operands[i];
						if (oper->type == IROperand::VREG) {
							oper->size = instruction_buffer->vregs[oper->value].size;
						}
					}

					instruction_buffer->entry_count++;
				}

				inline block_id_t current_block() const { return current_block_id; }
				inline void current_block(block_id_t block_id) { current_block_id = block_id; }

				inline block_id_t alloc_block() {
					return instruction_buffer->block_count++;
				}

				inline reg_id_t alloc_reg(uint8_t size) {
					instruction_buffer->vregs[instruction_buffer->vreg_count].size = size;
					return instruction_buffer->vreg_count++;
				}

			private:
				block_id_t current_block_id;

				struct instruction_entry {
					block_id_t block_id;
					IRInstruction instruction;
				};

				struct vreg_entry {
					uint8_t size;
				};

				struct bytecode_descriptor {
					uint32_t block_count;
					uint32_t vreg_count;
					struct vreg_entry vregs[1024];
					uint32_t entry_count;
					struct instruction_entry entries[];
				} *instruction_buffer;
			};
		}
	}
}

#endif	/* TRANSLATION_CONTEXT_H */
