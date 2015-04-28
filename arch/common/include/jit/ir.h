/*
 * File:   ir.h
 * Author: spink
 *
 * Created on 28 April 2015, 14:50
 */

#ifndef IR_H
#define	IR_H

#include <shared-jit.h>
#include <map>

namespace captive {
	namespace arch {
		namespace jit {
			class IRBlock;
			class IRContext;

			class IRRegister
			{
			public:
				IRRegister(IRContext& owner);
			};

			class IRInstruction
			{
			public:
				IRInstruction(IRBlock& owner);

			private:
				IRBlock& _owner;
			};

			class IRBlock
			{
			public:
				IRBlock(IRContext& owner);
				~IRBlock();

				inline const std::list<IRInstruction *>& instructions() const { return _instructions; }

			private:
				IRContext& _owner;
				std::list<IRInstruction *> _instructions;
			};

			class IRContext
			{
			public:
				IRContext();
				~IRContext();

				IRBlock& get_block_by_id(shared::IRBlockId id);

			private:
				typedef std::map<shared::IRBlockId, IRBlock *> block_map_t;
				block_map_t _blocks;
			};
		}
	}
}

#endif	/* IR_H */

