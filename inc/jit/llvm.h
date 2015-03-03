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

#include <map>

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

			virtual void *compile_block(const RawBytecodeDescriptor* bcd) override;
			virtual void *compile_region(const RawBytecodeDescriptor* bcd) override;

		private:
			struct LoweringContext
			{
				LoweringContext(llvm::IRBuilder<>& _builder) : builder(_builder) { }

				llvm::IRBuilder<>& builder;

				std::map<uint32_t, llvm::BasicBlock *> basic_blocks;
				std::map<uint32_t, llvm::Value *> vregs;

				llvm::Value *regs;

				llvm::Type *i8;
				llvm::Type *i16;
				llvm::Type *i32;
				llvm::Type *i64;

				inline llvm::Value *const8(uint8_t v)
				{
					return llvm::ConstantInt::get(i8, v);
				}

				inline llvm::Value *const16(uint16_t v)
				{
					return llvm::ConstantInt::get(i16, v);
				}

				inline llvm::Value *const32(uint32_t v)
				{
					return llvm::ConstantInt::get(i32, v);
				}

				inline llvm::Value *const64(uint64_t v)
				{
					return llvm::ConstantInt::get(i8, v);
				}
			};

			llvm::Value *value_for_operand(LoweringContext& ctx, const RawOperand* oper);
			llvm::Value *vreg_for_operand(LoweringContext& ctx, const RawOperand* oper);
			llvm::BasicBlock *block_for_operand(LoweringContext& ctx, const RawOperand* oper);

			bool lower_bytecode(LoweringContext& ctx, const RawBytecode* bc);

			LLVMJITMemoryManager *mm;
		};
	}
}

#endif	/* LLVM_H */

