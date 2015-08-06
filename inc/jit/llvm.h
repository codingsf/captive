/*
 * File:   llvm.h
 * Author: spink
 *
 * Created on 02 March 2015, 09:12
 */

#ifndef LLVM_H
#define	LLVM_H

#include <string>
#include <set>

#include <jit/jit.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/PassManager.h>

#include <shared-jit.h>

namespace llvm {
	class LLVMContext;
	class Type;
	class Module;
	class Function;
	class BasicBlock;
	class Pass;
}

namespace captive {
	namespace engine {
		class Engine;
	}

	namespace util {
		class ThreadPool;
	}
	
	namespace shared {
		class IRInstruction;
	}

	namespace jit {
		class LLVMJIT : public JIT {
		public:
			LLVMJIT(engine::Engine& engine, util::ThreadPool& worker_threads);
			virtual ~LLVMJIT();

			bool init() override;
			
			void *compile_region(shared::RegionWorkUnit *rwu) override;
			
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
					llvm::Type *voidty, *jit_state_ty, *jit_state_ptr;
				} types;
				
				uint32_t phys_region_index;
				
				llvm::Module *rgn_module;
				llvm::Function *rgn_func;
				
				llvm::BasicBlock *entry_block;
				llvm::BasicBlock *exit_normal_block;
				llvm::BasicBlock *exit_handle_block;
				llvm::BasicBlock *dispatch_block;
				llvm::BasicBlock *alloca_block;
				llvm::BasicBlock *chain_block;
				
				llvm::Value *jit_state;
				llvm::Value *cpu_obj;
				llvm::Value *reg_state;
				llvm::Value *region_table;
				llvm::Value *entry_page;
				llvm::Value *exit_code_var;
				llvm::Value *insn_counter;
				llvm::Value *isr;
				llvm::Value *pc_ptr;
				
				llvm::Constant *jump_table_entries[2048];
				llvm::GlobalVariable *jump_table;
				
				std::map<uint32_t, llvm::BasicBlock *> guest_basic_blocks;
				std::set<uint32_t> direct_blocks;
				
				inline llvm::ConstantInt *constu8(uint8_t v) { return (llvm::ConstantInt *)llvm::ConstantInt::get(types.i8, v, false); }
				inline llvm::ConstantInt *constu16(uint16_t v) { return (llvm::ConstantInt *)llvm::ConstantInt::get(types.i16, v, false); }
				inline llvm::ConstantInt *constu32(uint32_t v) { return (llvm::ConstantInt *)llvm::ConstantInt::get(types.i32, v, false); }
				inline llvm::ConstantInt *constu64(uint64_t v) { return (llvm::ConstantInt *)llvm::ConstantInt::get(types.i64, v, false); }
				
				inline llvm::ConstantInt *consti1(int8_t v) { return (llvm::ConstantInt *)llvm::ConstantInt::get(types.i1, v, true); }
				inline llvm::ConstantInt *consti8(int8_t v) { return (llvm::ConstantInt *)llvm::ConstantInt::get(types.i8, v, true); }
				inline llvm::ConstantInt *consti16(int16_t v) { return (llvm::ConstantInt *)llvm::ConstantInt::get(types.i16, v, true); }
				inline llvm::ConstantInt *consti32(int32_t v) { return (llvm::ConstantInt *)llvm::ConstantInt::get(types.i32, v, true); }
				inline llvm::ConstantInt *consti64(int64_t v) { return (llvm::ConstantInt *)llvm::ConstantInt::get(types.i64, v, true); }
			};
			
			struct BlockCompilationContext
			{
				BlockCompilationContext(RegionCompilationContext& rcc, uint32_t block_offset) : rcc(rcc), builder(rcc.builder), block_offset(block_offset) { }
				
				RegionCompilationContext& rcc;
				llvm::IRBuilder<>& builder;

				uint32_t block_offset;
				
				llvm::BasicBlock *alloca_block;
				
				std::map<uint32_t, llvm::BasicBlock *> ir_blocks;
				std::map<uint32_t, llvm::Value *> ir_vregs;
			};
			
			void print_module(std::string filename, llvm::Module *module);
			bool add_pass(llvm::PassManagerBase *pm, llvm::Pass *pass);
			bool initialise_pass_manager(llvm::PassManagerBase *pm);
			
			bool lower_block(RegionCompilationContext& rcc, const shared::BlockWorkUnit *bwu);
			
			enum metadata_tags
			{
				AA_MD_NO_ALIAS	= 0,
				AA_MD_VREG		= 1,
				AA_MD_REGISTER	= 2,
				AA_MD_MEMORY	= 3,
				AA_MD_ISR		= 4,
				AA_MD_JIT_STATE	= 5,
				AA_MD_JT_ELEM	= 6,
				AA_MD_INSN_COUNTER	= 7,
			};
			
			void set_aa_metadata(llvm::Value *v, metadata_tags tag);
			void set_aa_metadata(llvm::Value *v, metadata_tags tag, llvm::Value *value);
			
			llvm::Value *value_for_operand(BlockCompilationContext& bcc, const shared::IROperand *oper);
			llvm::Type *type_for_operand(BlockCompilationContext& bcc, const shared::IROperand* oper, bool ptr);
			
			llvm::Value *get_ir_vreg(BlockCompilationContext& bcc, shared::IRRegId id, uint8_t size);
			llvm::Value *vreg_for_operand(BlockCompilationContext& bcc, const shared::IROperand* oper);
			
			llvm::BasicBlock *get_ir_block(BlockCompilationContext& bcc, shared::IRBlockId id);
			llvm::BasicBlock *block_for_operand(BlockCompilationContext& bcc, const shared::IROperand* oper);
			
			bool lower_instruction(BlockCompilationContext& bcc, const shared::IRInstruction *insn);
		};
	}
}

#endif	/* LLVM_H */
