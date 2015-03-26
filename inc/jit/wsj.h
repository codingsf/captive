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

	namespace hypervisor {
		class SharedMemory;
	}

	namespace jit {
		class IRContext;

		namespace x86 {
			class X86Builder;
		}

		namespace ir {
			class IRBlock;
		}

		class WSJ : public JIT, public BlockJIT, public RegionJIT
		{
		public:
			WSJ(engine::Engine& engine, util::ThreadPool& worker_threads);
			virtual ~WSJ();

			virtual bool init() override;

			virtual BlockJIT& block_jit() override { return *this; }
			virtual RegionJIT& region_jit() override { return *this; }

			BlockCompilationResult compile_block(BlockWorkUnit *bwu) override;
			RegionCompilationResult compile_region(RegionWorkUnit *rwu) override;

		private:
			engine::Engine& _engine;

			bool build(x86::X86Builder& builder, const RawBytecodeDescriptor* bcd);
			bool lower_block(x86::X86Builder& builder, IRContext& ctx, ir::IRBlock& block);
		};
	}
}

#endif	/* WSJ_H */

