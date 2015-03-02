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
					instruction_buffer[next_instruction].block_id = block_id;
					instruction_buffer[next_instruction].instruction = instruction;

					next_instruction++;
				}

				inline block_id_t current_block() const { return current_block_id; }
				inline void current_block(block_id_t block_id) { current_block_id = block_id; }

				inline block_id_t alloc_block() {
					return next_block_id++;
				}

				inline reg_id_t alloc_reg(int size) {
					return next_reg_id++;
				}

			private:
				block_id_t next_block_id;
				block_id_t current_block_id;
				reg_id_t next_reg_id;
				uint32_t next_instruction;

				struct instruction_entry {
					block_id_t block_id;
					IRInstruction instruction;
				} *instruction_buffer;
			};
		}
	}
}

#endif	/* TRANSLATION_CONTEXT_H */

