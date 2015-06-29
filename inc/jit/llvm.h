/*
 * File:   llvm.h
 * Author: spink
 *
 * Created on 02 March 2015, 09:12
 */

#ifndef LLVM_H
#define	LLVM_H

#include <jit/jit.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>

namespace llvm {
	class LLVMContext;
	class Type;
	class Module;
	class Function;
	class BasicBlock;
}

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
			
			void *compile_region(uint32_t gpa, const std::vector<std::pair<uint32_t, shared::BlockTranslation *>>& blocks) override;
			
		private:
			engine::Engine& _engine;
			
			struct RegionCompilationContext
			{
				RegionCompilationContext() : builder(ctx) { }
				llvm::LLVMContext ctx;
				llvm::IRBuilder<> builder;
				
				struct {
					llvm::Type *i1, *i8, *i16, *i32, *i64;
					llvm::Type *pi1, *pi8, *pi16, *pi32, *pi64;
					llvm::Type *voidty, *jit_state;
				} types;
				
				llvm::Module *rgn_module;
				llvm::Function *rgn_func;
				
				llvm::BasicBlock *entry_block;
				llvm::BasicBlock *exit_block;
				llvm::BasicBlock *dispatch_block;
				
				inline llvm::Value *constu8(uint8_t v) { return llvm::ConstantInt::get(types.i8, v, false); }
				inline llvm::Value *constu16(uint16_t v) { return llvm::ConstantInt::get(types.i16, v, false); }
				inline llvm::Value *constu32(uint32_t v) { return llvm::ConstantInt::get(types.i32, v, false); }
				inline llvm::Value *constu64(uint64_t v) { return llvm::ConstantInt::get(types.i64, v, false); }
				
				inline llvm::Value *consti8(int8_t v) { return llvm::ConstantInt::get(types.i8, v, true); }
				inline llvm::Value *consti16(int16_t v) { return llvm::ConstantInt::get(types.i16, v, true); }
				inline llvm::Value *consti32(int32_t v) { return llvm::ConstantInt::get(types.i32, v, true); }
				inline llvm::Value *consti64(int64_t v) { return llvm::ConstantInt::get(types.i64, v, true); }
			};
		};
	}
}

#endif	/* LLVM_H */
