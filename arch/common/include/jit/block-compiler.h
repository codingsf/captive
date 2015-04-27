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

			class BlockCompiler
			{
			public:
				BlockCompiler();
				bool compile(shared::TranslationBlock& tb, block_txln_fn& fn);

			private:
				class ControlFlowGraph
				{
				public:
					bool build(const shared::TranslationBlock& tb);

					inline const std::set<shared::IRBlockId>& blocks() const { return all_blocks; }
					inline const std::vector<shared::IRBlockId>& predecessors(shared::IRBlockId block) const { return block_preds.find(block)->second; }
					inline const std::vector<shared::IRBlockId>& successors(shared::IRBlockId block) const { return block_succs.find(block)->second; }

				private:
					std::set<shared::IRBlockId> all_blocks;
					std::map<shared::IRBlockId, std::vector<shared::IRBlockId>> block_preds;
					std::map<shared::IRBlockId, std::vector<shared::IRBlockId>> block_succs;
				};

				bool optimise(shared::TranslationBlock& tb);
				bool allocate(shared::TranslationBlock& tb);
				bool lower(shared::TranslationBlock& tb, block_txln_fn& fn);

				bool merge_blocks(shared::TranslationBlock& tb, const ControlFlowGraph& cfg);
			};
		}
	}
}

#endif	/* BLOCK_COMPILER_H */

