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
#include <jit/ir-block.h>
#include <jit/ir-instruction.h>

namespace captive {
	namespace arch {
		namespace jit {
			class TranslationContext
			{
			public:
				TranslationContext();

				GuestBasicBlock::GuestBasicBlockFn compile();

				inline IRBlock *create_block() {
					nr_blocks++;
					blocks = (IRBlock **)captive::arch::realloc(blocks, sizeof(IRBlock *) * nr_blocks);

					IRBlock *block = new IRBlock();

					block->attach(*this);
					blocks[nr_blocks-2] = block;
				}

				IRInstruction *create_instruction(IRInstruction::IRInstructionType type) {
					assert(current_block);

					IRInstruction *insn;
					switch (type) {
					case IRInstruction::ADD:
						insn = new IRAddInstruction();
						break;
					default:
						insn = NULL;
						break;
					}

					if (!insn) {
						return NULL;
					}

					current_block->add_instruction(*insn);
				}

			private:
				int nr_blocks;
				IRBlock **blocks;
				IRBlock *current_block;
			};
		}
	}
}

#endif	/* TRANSLATION_CONTEXT_H */

