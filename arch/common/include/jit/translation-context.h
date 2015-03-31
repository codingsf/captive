/*
 * File:   translation-context.h
 * Author: spink
 *
 * Created on 27 February 2015, 15:07
 */

#ifndef TRANSLATION_CONTEXT_H
#define	TRANSLATION_CONTEXT_H

#include <define.h>
#include <shared-memory.h>
#include <jit/guest-basic-block.h>
#include <jit/ir-instruction.h>

namespace captive {
	namespace arch {
		class SharedMemory;

		namespace jit {
			class TranslationContext
			{
			public:
				TranslationContext(SharedMemory& allocator);

				GuestBasicBlock::GuestBasicBlockFn compile();

				inline void add_instruction(const IRInstruction& instruction) {
					add_instruction(_current_block_id, instruction);
				}

				inline void add_instruction(block_id_t block_id, const IRInstruction& instruction) {
					ensure_buffer(instruction_buffer->entry_count + 1);

					instruction_buffer->entries[instruction_buffer->entry_count].block_id = block_id;
					instruction_buffer->entries[instruction_buffer->entry_count].instruction = instruction;
					instruction_buffer->entry_count++;
				}

				inline block_id_t current_block() const { return _current_block_id; }
				inline void current_block(block_id_t block_id) { _current_block_id = block_id; }

				inline block_id_t alloc_block() {
					return instruction_buffer->block_count++;
				}

				inline reg_id_t alloc_reg(uint8_t size) {
					return instruction_buffer->vreg_count++;
				}

				inline void *buffer() const { return instruction_buffer; }

			private:
				SharedMemory& _allocator;
				block_id_t _current_block_id;
				uint32_t _buffer_size;

				inline void ensure_buffer(uint32_t elem_capacity)
				{
					uint32_t required_size = (sizeof(struct instruction_entry) * elem_capacity) + sizeof(struct bytecode_descriptor);
					if (_buffer_size < required_size) {
						while (_buffer_size < required_size) {
							_buffer_size <<= 1;
						}

						instruction_buffer = (struct bytecode_descriptor *)_allocator.reallocate(instruction_buffer, _buffer_size);
					}
				}

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

