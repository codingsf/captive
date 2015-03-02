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
	namespace jit {
		class LLVMJIT : public JIT, public BlockJIT, public RegionJIT {
		public:
			virtual ~LLVMJIT();

			virtual bool init() override;


			virtual BlockJIT& block_jit() override { return *this; }
			virtual RegionJIT& region_jit() override { return *this; }

			virtual uint64_t compile_block(const RawBytecode* bc, uint32_t count) override;
			virtual uint64_t compile_region(const RawBytecode* bc, uint32_t count) override;
		};
	}
}

#endif	/* LLVM_H */

