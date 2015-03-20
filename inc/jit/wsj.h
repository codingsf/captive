/*
 * File:   wsj.h
 * Author: spink
 *
 * Created on 10 March 2015, 16:40
 */

#ifndef WSJ_H
#define	WSJ_H

#include <jit/jit.h>

namespace captive {
	namespace engine {
		class Engine;
	}

	namespace jit {
		class IRContext;

		namespace x86 {
			class X86Builder;
		}

		namespace ir {
			class IRBlock;
		}

		class Allocator;

		class WSJ : public JIT, public BlockJIT, public RegionJIT
		{
		public:
			WSJ(engine::Engine& engine);
			virtual ~WSJ();

			virtual bool init() override;

			virtual BlockJIT& block_jit() override { return *this; }
			virtual RegionJIT& region_jit() override { return *this; }

		protected:
			void *internal_compile_block(const RawBytecodeDescriptor* bcd) override;
			void *internal_compile_region(const RawBlockDescriptors* bds, const RawBytecodeDescriptor* bcd) override;

		private:
			engine::Engine& _engine;
			Allocator *_allocator;

			bool build(x86::X86Builder& builder, const RawBytecodeDescriptor* bcd);
			bool lower_block(x86::X86Builder& builder, IRContext& ctx, ir::IRBlock& block);
		};
	}
}

#endif	/* WSJ_H */

