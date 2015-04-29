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

#include <map>
#include <vector>
#include <set>

namespace captive {
	namespace arch {
		namespace jit {
			typedef uint32_t (*block_txln_fn)(void *);

			class IRContext;
			class IRInstruction;
			class IROperand;

			class BlockCompiler
			{
			public:
				BlockCompiler();
				bool compile(shared::TranslationBlock& tb, block_txln_fn& fn);

			private:
				bool build(shared::TranslationBlock& tb, IRContext& ctx);
				bool optimise(IRContext& ctx);
				bool allocate(IRContext& ctx);
				bool lower(IRContext& ctx, block_txln_fn& fn);

				IRInstruction *instruction_from_shared(IRContext& ctx, const shared::IRInstruction *insn);
				IROperand *operand_from_shared(IRContext& ctx, const shared::IROperand *operand);
			};
		}
	}
}

#endif	/* BLOCK_COMPILER_H */

