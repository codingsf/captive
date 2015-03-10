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
				TranslationContext(void *instruction_buffer, uint64_t instruction_buffer_size, uint64_t instruction_buffer_offset, uint64_t code_buffer_offset);

				GuestBasicBlock::GuestBasicBlockFn compile();

				inline void add_instruction(const IRInstruction& instruction) {
					add_instruction(current_block_id, instruction);
				}

				inline void add_instruction(block_id_t block_id, const IRInstruction& instruction) {
					instruction_buffer->entries[instruction_buffer->entry_count].block_id = block_id;
					instruction_buffer->entries[instruction_buffer->entry_count].instruction = instruction;
					instruction_buffer->entry_count++;
				}

				inline block_id_t current_block() const { return current_block_id; }
				inline void current_block(block_id_t block_id) { current_block_id = block_id; }

				inline block_id_t alloc_block() {
					return instruction_buffer->block_count++;
				}

				inline reg_id_t alloc_reg(uint8_t size) {
					return instruction_buffer->vreg_count++;
				}

			private:
				block_id_t current_block_id;
				uint64_t instruction_buffer_size;
				uint64_t instruction_buffer_offset;
				uint64_t code_buffer_offset;

				struct instruction_entry {
					block_id_t block_id;
					IRInstruction instruction;
				} packed;

				struct vreg_entry {
					uint8_t size;
				} packed;

				struct bytecode_descriptor {
					uint32_t block_count;
					uint32_t vreg_count;
					uint32_t entry_count;
					struct instruction_entry entries[];
				} packed *instruction_buffer;
			};
		}
	}
}

#endif	/* TRANSLATION_CONTEXT_H */

