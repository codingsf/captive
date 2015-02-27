/*
 * File:   ir-block.h
 * Author: spink
 *
 * Created on 27 February 2015, 16:32
 */

#ifndef IR_BLOCK_H
#define	IR_BLOCK_H

#include <jit/ir-instruction.h>

namespace captive {
	namespace arch {
		namespace jit {
			class TranslationContext;
			
			class IRBlock {
			public:
				IRBlock();

				inline void attach(TranslationContext& ctx) {
					_parent = &ctx;
				}

				inline void add_instruction(IRInstruction& insn) {
					insn.attach(*this);
				}

			private:
				TranslationContext *_parent;
			};
		}
	}
}

#endif	/* IR_BLOCK_H */

