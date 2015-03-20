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
	namespace engine {
		class Engine;
	}

	namespace jit {
		class LLVMJITMemoryManager;
		class Allocator;

		class LLVMJIT : public JIT, public BlockJIT, public RegionJIT {
		public:
			LLVMJIT(engine::Engine& engine);
			virtual ~LLVMJIT();

			virtual bool init() override;

			virtual BlockJIT& block_jit() override { return *this; }
			virtual RegionJIT& region_jit() override { return *this; }

		protected:
			void *internal_compile_block(const RawBytecodeDescriptor* bcd) override;
			void *internal_compile_region(const RawBlockDescriptors* bds, const RawBytecodeDescriptor* bcd) override;

		private:
			engine::Engine& _engine;
			Allocator *_allocator;

			struct LoweringContext
			{
				LoweringContext(llvm::IRBuilder<>& _builder) : builder(_builder) { }

				llvm::IRBuilder<>& builder;

				llvm::BasicBlock *alloca_block;

				std::map<uint32_t, llvm::BasicBlock *> basic_blocks;
				std::map<uint32_t, llvm::Value *> vregs;

				llvm::Value *cpu_obj;
				llvm::Value *reg_state;

				llvm::Type *vtype;
				llvm::Type *i1;
				llvm::Type *i8, *pi8;
				llvm::Type *i16, *pi16;
				llvm::Type *i32, *pi32;
				llvm::Type *i64, *pi64;

				inline llvm::Value *const1(uint8_t v)
				{
					return llvm::ConstantInt::get(i1, v);
				}

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
					return llvm::ConstantInt::get(i64, v);
				}
			};

			llvm::Value *insert_vreg(LoweringContext& ctx, uint32_t idx, uint8_t size);
			llvm::Value *value_for_operand(LoweringContext& ctx, const RawOperand* oper);
			llvm::Type *type_for_operand(LoweringContext& ctx, const RawOperand* oper, bool ptr = false);
			llvm::Value *vreg_for_operand(LoweringContext& ctx, const RawOperand* oper);
			llvm::BasicBlock *block_for_operand(LoweringContext& ctx, const RawOperand* oper);

			bool lower_bytecode(LoweringContext& ctx, const RawBytecode* bc);
		};
	}
}

#endif	/* LLVM_H */

