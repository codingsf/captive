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
#include <llvm/PassManager.h>

#include <shared-jit.h>

#include <map>

namespace captive {
	namespace engine {
		class Engine;
	}

	namespace util {
		class ThreadPool;
	}

	namespace jit {
		class LLVMJITMemoryManager;

		class LLVMJIT : public JIT, public BlockJIT, public RegionJIT {
		public:
			LLVMJIT(engine::Engine& engine, util::ThreadPool& worker_threads);
			virtual ~LLVMJIT();

			bool init() override;

			BlockJIT& block_jit() override { return *this; }
			RegionJIT& region_jit() override { return *this; }

			bool compile_block(shared::BlockWorkUnit *bwu) override;
			bool compile_region(shared::RegionWorkUnit *rwu) override;

		private:
			engine::Engine& _engine;

			struct LoweringContext
			{
				LoweringContext(llvm::IRBuilder<>& _builder, const shared::RegionWorkUnit& _rwu) : builder(_builder), rwu(_rwu) { }

				llvm::IRBuilder<>& builder;
				const shared::RegionWorkUnit& rwu;

				llvm::Function *region_fn;

				llvm::BasicBlock *dispatch_block;
				llvm::BasicBlock *chain_block;
				llvm::BasicBlock *exit_block;

				std::map<uint32_t, llvm::BasicBlock *> guest_block_entries;

				llvm::Value *cpu_obj;
				llvm::Value *reg_state;
				llvm::Value *region_chain_tbl;
				llvm::Value *pc_ptr;
				llvm::Value *virtual_base_address;

				llvm::Type *vtype;
				llvm::Type *i1;
				llvm::Type *i8, *pi8;
				llvm::Type *i16, *pi16;
				llvm::Type *i32, *pi32;
				llvm::Type *i64, *pi64;

				inline llvm::ConstantInt *const1(uint8_t v)
				{
					return (llvm::ConstantInt *)llvm::ConstantInt::get(i1, v);
				}

				inline llvm::ConstantInt *const8(uint8_t v)
				{
					return (llvm::ConstantInt *)llvm::ConstantInt::get(i8, v);
				}

				inline llvm::ConstantInt *const16(uint16_t v)
				{
					return (llvm::ConstantInt *)llvm::ConstantInt::get(i16, v);
				}

				inline llvm::ConstantInt *const32(uint32_t v)
				{
					return (llvm::ConstantInt *)llvm::ConstantInt::get(i32, v);
				}

				inline llvm::ConstantInt *const64(uint64_t v)
				{
					return (llvm::ConstantInt *)llvm::ConstantInt::get(i64, v);
				}

				inline llvm::Value *nullptr8()
				{
					return builder.CreateIntToPtr(const64(0), pi8);
				}

				inline llvm::Value *materialise(uint32_t offset)
				{
					assert(virtual_base_address);
					return builder.CreateAdd(virtual_base_address, const32(offset));
				}
			};

			struct BlockLoweringContext
			{
				LoweringContext& parent;
				llvm::IRBuilder<>& builder;
				const shared::TranslationBlock& tb;

				llvm::BasicBlock *alloca_block;

				std::map<uint32_t, llvm::Value *> ir_vregs;
				std::map<uint32_t, llvm::BasicBlock *> ir_blocks;

				BlockLoweringContext(LoweringContext& parent, const shared::TranslationBlock& tb) : parent(parent), builder(parent.builder), tb(tb) { }
			};

			enum metadata_tags
			{
				TAG_CLASS_MEMORY  = 1,
				TAG_CLASS_REGISTER = 2,
				TAG_CLASS_ALLOC = 3,
				TAG_CLASS_CHAIN = 4,
			};

			void set_aa_metadata(llvm::Value *inst, metadata_tags tag);
			void set_aa_metadata(llvm::Value *inst, metadata_tags tag, llvm::Value *value);
			bool add_pass(llvm::PassManagerBase *pm, llvm::Pass *pass);
			bool initialise_pass_manager(llvm::PassManagerBase *pm);

			llvm::Value *insert_vreg(BlockLoweringContext& ctx, uint32_t idx, uint8_t size);
			llvm::Value *value_for_operand(BlockLoweringContext& ctx, const shared::IROperand* oper);
			llvm::Type *type_for_operand(BlockLoweringContext& ctx, const shared::IROperand* oper, bool ptr = false);
			llvm::Value *vreg_for_operand(BlockLoweringContext& ctx, const shared::IROperand* oper);
			llvm::BasicBlock *block_for_operand(BlockLoweringContext& ctx, const shared::IROperand* oper);

			bool lower_block(LoweringContext& ctx, const shared::TranslationBlock *block);
			bool lower_ir_instruction(BlockLoweringContext& ctx, const shared::IRInstruction *insn);
			bool emit_block_control_flow(BlockLoweringContext& ctx, const shared::IRInstruction *insn);
			bool emit_interrupt_check(BlockLoweringContext& ctx);

			void print_module(std::string filename, llvm::Module* module);
		};
	}
}

#endif	/* LLVM_H */

