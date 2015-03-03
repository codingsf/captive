/*
 * File:   llvm.h
 * Author: spink
 *
 * Created on 02 March 2015, 09:12
 */

#ifndef LLVM_H
#define	LLVM_H

#include <jit/jit.h>

#include <llvm/IR/IRBuilder.h>

namespace captive {
	namespace jit {
		class LLVMJITMemoryManager;

		class LLVMJIT : public JIT, public BlockJIT, public RegionJIT {
		public:
			LLVMJIT();
			virtual ~LLVMJIT();

			virtual bool init() override;

			virtual BlockJIT& block_jit() override { return *this; }
			virtual RegionJIT& region_jit() override { return *this; }

			virtual void *compile_block(const RawBytecode* bc, uint32_t count) override;
			virtual void *compile_region(const RawBytecode* bc, uint32_t count) override;

		private:
			bool LowerBytecode(llvm::IRBuilder<>& builder, const RawBytecode* bc);
			LLVMJITMemoryManager *mm;
		};
	}
}

#endif	/* LLVM_H */

