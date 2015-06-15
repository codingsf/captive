#include <jit/llvm.h>
#include <jit/llvm-mm.h>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>

#include <llvm/Support/TargetSelect.h>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>

#include <llvm/PassManager.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/Passes.h>

#include <shared-jit.h>
#include <captive.h>

//#include "arch/page-jit.h"

USE_CONTEXT(JIT)
USE_CONTEXT(LLVM)
DECLARE_CHILD_CONTEXT(LLVMPageJIT, LLVM);

using namespace captive::jit;
using namespace captive::shared;
using namespace llvm;

bool LLVMJIT::compile_page(PageWorkUnit *pwu)
{
	LLVMContext ctx;
	PageCompilationContext pcc(pwu, ctx);

	pcc.page_module = new Module("page", ctx);

	pcc.vtype = Type::getVoidTy(ctx);
	pcc.i1 = IntegerType::getInt1Ty(ctx);
	pcc.i8 = IntegerType::getInt8Ty(ctx); pcc.pi8 = PointerType::get(pcc.i8, 0);
	pcc.i16 = IntegerType::getInt16Ty(ctx); pcc.pi16 = PointerType::get(pcc.i16, 0);
	pcc.i32 = IntegerType::getInt32Ty(ctx); pcc.pi32 = PointerType::get(pcc.i32, 0);
	pcc.i64 = IntegerType::getInt64Ty(ctx); pcc.pi64 = PointerType::get(pcc.i64, 0);

	std::vector<Type *> jit_state_elements;
	jit_state_elements.push_back(pcc.pi8);				// CPU
	jit_state_elements.push_back(pcc.pi8);				// REGISTERS
	jit_state_elements.push_back(pcc.i32);				// REGISTERS_SIZE
	jit_state_elements.push_back(pcc.pi8->getPointerTo(0));		// REGION CHAINING TABLE

	Type *jit_state_type = StructType::get(ctx, jit_state_elements);
	PointerType *jit_state_ptr_type = jit_state_type->getPointerTo(0);

	std::vector<Type *> params;
	params.push_back(jit_state_ptr_type);

	FunctionType *fn_type = FunctionType::get(pcc.i32, params, false);
	pcc.page_fn = Function::Create(fn_type, GlobalValue::ExternalLinkage, "page", pcc.page_module);
	pcc.page_fn->addFnAttr(Attribute::NoRedZone);

	Function::ArgumentListType& args = pcc.page_fn->getArgumentList();
	Value *jit_state_ptr_val = &args.front();

	// Create some structural blocks
	pcc.entry_block = BasicBlock::Create(ctx, "entry", pcc.page_fn);
	pcc.exit_block = BasicBlock::Create(ctx, "exit", pcc.page_fn);
	pcc.dispatch_block = BasicBlock::Create(ctx, "dispatch", pcc.page_fn);

	pcc.builder.SetInsertPoint(pcc.entry_block);

	// Initialise a pointer to the register state
	Value *jit_state_val = pcc.builder.CreateLoad(jit_state_ptr_val, "jit_state");
	pcc.cpu_obj = pcc.builder.CreateExtractValue(jit_state_val, {0}, "cpu_object");
	pcc.reg_state = pcc.builder.CreatePtrToInt(pcc.builder.CreateExtractValue(jit_state_val, {1}, "reg_state"), pcc.i64);

	// Initialise a pointer to the PC
	Value *pc_ptr_off = pcc.builder.CreateAdd(pcc.reg_state, pcc.const64(60));	// XXX HACK HACK HACK TODO FIXME
	pcc.pc_ptr = pcc.builder.CreateIntToPtr(pc_ptr_off, pcc.pi32);
	set_aa_metadata(pcc.pc_ptr, TAG_CLASS_REGISTER, pcc.const64(60));

	// Initialise the virtual base address which this page was entered with
	pcc.virtual_base_address = pcc.builder.CreateAnd(pcc.builder.CreateLoad(pcc.pc_ptr), ~0xfffULL);

	// Jump to the dispatcher
	pcc.builder.CreateBr(pcc.dispatch_block);

	// Build the dispatcher
	pcc.builder.SetInsertPoint(pcc.dispatch_block);
	Value *page_offset = pcc.builder.CreateAnd(pcc.builder.CreateLoad(pcc.pc_ptr), 0xfffULL);
	pcc.dispatcher = pcc.builder.CreateSwitch(page_offset, pcc.exit_block);

	// Build the exit block
	pcc.builder.SetInsertPoint(pcc.exit_block);
	pcc.builder.CreateRet(pcc.const32(0));

	for (int i = 0; i < 4096; i++) {
		if (pwu->entries[i / 8] & (1 << (i % 8))) {
			compile_page_block(pcc, i);
		}
	}

	/// -----------------------------------------------------------------------------------------

	{
		PassManager verifyManager;
		verifyManager.add(createVerifierPass(true));
		verifyManager.run(*pcc.page_module);
	}

	/// -----------------------------------------------------------------------------------------

	{
		std::stringstream filename;
		filename << "region-" << std::hex << (uint64_t)(1234) << ".opt.ll";
		print_module(filename.str(), pcc.page_module);
	}

	/// -----------------------------------------------------------------------------------------

	// Initialise a new MCJIT engine
	TargetOptions target_opts;
	target_opts.DisableTailCalls = false;
	target_opts.PositionIndependentExecutable = true;
	target_opts.EnableFastISel = false;
	target_opts.NoFramePointerElim = true;
	target_opts.PrintMachineCode = false;

	ExecutionEngine *engine = EngineBuilder(std::unique_ptr<Module>(pcc.page_module))
		.setEngineKind(EngineKind::JIT)
		//.setMCJITMemoryManager(std::unique_ptr<RTDyldMemoryManager>(new LLVMJITMemoryManager(_engine, *_shared_memory)))
		.setTargetOptions(target_opts)
		.create();

	if (!engine) {
		ERROR << CONTEXT(LLVMPageJIT) << "Unable to create LLVM Engine";
		return false;
	}

	// We actually want compilation to happen.
	engine->DisableLazyCompilation();
	engine->finalizeObject();

	// Compile the function!
	void *ptr = (void *)engine->getFunctionAddress(pcc.page_fn->getName());
	if (!ptr) {
		ERROR << CONTEXT(LLVMPageJIT) << "Unable to compile function";
		return false;
	}

	pcc.page_fn->deleteBody();
	delete engine;

	//rwu->function_addr = (uint64_t)ptr;

	return false;
}

void LLVMJIT::compile_page_block(PageCompilationContext& pcc, uint32_t offset)
{
	fprintf(stderr, "compiling page block: %x\n", offset);

	BasicBlock *page_block = BasicBlock::Create(pcc.ctx, "block-" + std::to_string(offset), pcc.page_fn);
	pcc.dispatcher->addCase(pcc.const32(offset), page_block);

	pcc.builder.SetInsertPoint(page_block);

	/*captive::arch::arm::PageJitGenerator pjg(pcc);
	pjg.generate(&pcc.pwu->page[offset]);*/

	pcc.builder.CreateBr(pcc.dispatch_block);
}
