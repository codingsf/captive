/*
 * File:   llvm.h
 * Author: spink
 *
 * Created on 02 March 2015, 09:12
 */

#ifndef LLVM_H
#define	LLVM_H

#include <jit/jit.h>

namespace captive {
	namespace engine {
		class Engine;
	}

	namespace util {
		class ThreadPool;
	}

	namespace jit {
		class LLVMJIT : public JIT {
		public:
			LLVMJIT(engine::Engine& engine, util::ThreadPool& worker_threads);
			virtual ~LLVMJIT();

			bool init() override;
			
			void compile_region_async(uint32_t gpa, const std::vector<uint32_t>& blocks) override;
			
		private:
			engine::Engine& _engine;
		};
	}
}

#endif	/* LLVM_H */
