#include <jit/llvm.h>
#include <jit/allocator.h>
#include <jit/llvm-mm.h>
#include <captive.h>

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

#include <fstream>

USE_CONTEXT(JIT)
USE_CONTEXT(LLVM)
DECLARE_CHILD_CONTEXT(LLVMRegionJIT, LLVM);

using namespace captive::jit;
using namespace llvm;

void *LLVMJIT::internal_compile_region(const RawBlockDescriptors* bd, const RawBytecodeDescriptor* bcd)
{
	//DEBUG << "Compiling Region with " << bd->block_count << " basic blocks.";

	if (bd->block_count == 0)
		return NULL;

	if (bcd->bytecode_count == 0)
		return NULL;

	/*for (unsigned int i = 0; i < bd->block_count; i++) {
		DEBUG << "GBB: addr=" << std::hex << bd->blocks[i].addr << ", id=" << std::dec << bd->blocks[i].id << ", heat=" << bd->blocks[i].heat;
	}*/

	LLVMContext ctx;
	Module *region_module = new Module("region", ctx);

	std::vector<Type *> params;
	params.push_back(IntegerType::getInt8PtrTy(ctx));
	params.push_back(IntegerType::getInt8PtrTy(ctx));

	FunctionType *fn = FunctionType::get(IntegerType::getInt32Ty(ctx), params, false);
	Function *region_fn = Function::Create(fn, GlobalValue::ExternalLinkage, "region", region_module);

	region_fn->addFnAttr(Attribute::NoRedZone);

	BasicBlock *entry_block = BasicBlock::Create(ctx, "entry", region_fn);
	BasicBlock *dispatch_block = BasicBlock::Create(ctx, "dispatch", region_fn);
	BasicBlock *exit_block = BasicBlock::Create(ctx, "exit", region_fn);

	IRBuilder<> builder(ctx);

	LoweringContext lc(builder);
	lc.vtype = Type::getVoidTy(ctx);
	lc.i1 = IntegerType::getInt1Ty(ctx);
	lc.i8 = IntegerType::getInt8Ty(ctx); lc.pi8 = PointerType::get(lc.i8, 0);
	lc.i16 = IntegerType::getInt16Ty(ctx); lc.pi16 = PointerType::get(lc.i16, 0);
	lc.i32 = IntegerType::getInt32Ty(ctx); lc.pi32 = PointerType::get(lc.i32, 0);
	lc.i64 = IntegerType::getInt64Ty(ctx); lc.pi64 = PointerType::get(lc.i64, 0);

	builder.SetInsertPoint(exit_block);
	builder.CreateRet(lc.const32(1));

	Function::ArgumentListType& args = region_fn->getArgumentList();

	lc.cpu_obj = &args.front();

	builder.SetInsertPoint(entry_block);
	lc.reg_state = builder.CreatePtrToInt(args.getNext(&args.front()), lc.i64);

	Value *pc_ptr_off = builder.CreateAdd(lc.reg_state, lc.const64(60));
	lc.pc_ptr = builder.CreateIntToPtr(pc_ptr_off, lc.pi32);
	lc.virtual_base_address = builder.CreateAnd(builder.CreateLoad(lc.pc_ptr), ~0xfffULL);

	// Populate the basic-block map
	for (uint32_t idx = 0; idx < bcd->bytecode_count; idx++) {
		if (lc.basic_blocks[bcd->bc[idx].block_id] == NULL) {
			lc.basic_blocks[bcd->bc[idx].block_id] = BasicBlock::Create(ctx, "bb", region_fn);
		}
	}

	lc.alloca_block = entry_block;
	lc.dispatch_block = dispatch_block;

	// Lower all instructions
	for (uint32_t idx = 0; idx < bcd->bytecode_count; idx++) {
		const RawBytecode *bc = &bcd->bc[idx];
		BasicBlock *bb = lc.basic_blocks[bc->block_id];

		builder.SetInsertPoint(bb);
		if (!lower_bytecode(lc, bc)) {
			ERROR << CONTEXT(LLVMRegionJIT) << "Failed to lower byte-code: " << bc->render();
			return NULL;
		}
	}

	// Create a branch to the first block, from the entry block.
	builder.SetInsertPoint(entry_block);
	builder.CreateBr(dispatch_block);

	builder.SetInsertPoint(dispatch_block);

	{
		std::vector<Type *> params;
		params.push_back(lc.pi8);

		FunctionType *fntype = FunctionType::get(lc.vtype, params, false);
		Constant *fn = builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("cpu_check_interrupts", fntype);
		builder.CreateCall(fn, lc.cpu_obj);
	}

	BasicBlock *do_dispatch_block = BasicBlock::Create(ctx, "do_dispatch", region_fn);

	Value *pc_region = builder.CreateAnd(builder.CreateLoad(lc.pc_ptr), ~0xfffULL);
	builder.CreateCondBr(builder.CreateICmpNE(pc_region, lc.virtual_base_address), exit_block, do_dispatch_block);

	builder.SetInsertPoint(do_dispatch_block);
	Value *pc_offset = builder.CreateAnd(builder.CreateLoad(lc.pc_ptr), 0xfff);
	SwitchInst *dispatcher = builder.CreateSwitch(pc_offset, exit_block);

	for (unsigned int i = 0; i < bd->block_count; i++) {
		dispatcher->addCase(lc.const32(bd->blocks[i].addr & 0xfff), lc.basic_blocks[bd->blocks[i].id]);
	}

	// Verify
	{
		PassManager verifyManager;
		verifyManager.add(createVerifierPass(true));
		verifyManager.run(*region_module);

	}

	// Print out the module
	/*{
		raw_os_ostream str(std::cerr);

		PassManager printManager;
		printManager.add(createPrintModulePass(str, ""));
		printManager.run(*block_module);
	}*/

	// Optimise
	{
		PassManager optManager;

		PassManagerBuilder optManagerBuilder;
		optManagerBuilder.BBVectorize = false;
		optManagerBuilder.DisableTailCalls = true;
		optManagerBuilder.OptLevel = 3;
		optManagerBuilder.populateModulePassManager(optManager);

		optManager.run(*region_module);
	}

	// Print out the module
	/*{
		std::stringstream filename;
		filename << "region-" << std::hex << (uint64_t)(bd->blocks[0].addr & ~0xfffULL) << ".ll";
		std::ofstream file(filename.str());
		raw_os_ostream str(file);

		PassManager printManager;
		printManager.add(createPrintModulePass(str, ""));
		printManager.run(*region_module);
	}*/

	if (_allocator == NULL) {
		_allocator = new Allocator(_code_arena, _code_arena_size);
	}

	// Initialise a new MCJIT engine
	TargetOptions target_opts;
	target_opts.DisableTailCalls = false;
	target_opts.PositionIndependentExecutable = true;
	target_opts.EnableFastISel = false;
	target_opts.NoFramePointerElim = true;
	target_opts.PrintMachineCode = false;

	ExecutionEngine *engine = EngineBuilder(std::unique_ptr<Module>(region_module))
		.setEngineKind(EngineKind::JIT)
		.setMCJITMemoryManager(std::unique_ptr<RTDyldMemoryManager>(new LLVMJITMemoryManager(_engine, *_allocator)))
		.setTargetOptions(target_opts)
		.create();

	if (!engine) {
		ERROR << CONTEXT(LLVMRegionJIT) << "Unable to create LLVM Engine";
		return NULL;
	}

	// We actually want compilation to happen.
	engine->DisableLazyCompilation();
	engine->finalizeObject();

	// Compile the function!
	void *ptr = (void *)engine->getFunctionAddress(region_fn->getName());
	if (!ptr) {
		ERROR << CONTEXT(LLVMRegionJIT) << "Unable to compile function";
		return NULL;
	}

	region_fn->deleteBody();
	delete engine;

	return ptr;
}

