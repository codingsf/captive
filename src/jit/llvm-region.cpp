#include <jit/llvm.h>
#include <jit/llvm-mm.h>
#include <shared-jit.h>
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

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/Passes.h>

#include <fstream>

USE_CONTEXT(JIT)
USE_CONTEXT(LLVM)
DECLARE_CHILD_CONTEXT(LLVMRegionJIT, LLVM);

using namespace captive::jit;
using namespace captive::shared;
using namespace llvm;

bool LLVMJIT::compile_region(RegionWorkUnit *rwu)
{
	if (!rwu->valid)
		return false;

	DEBUG << CONTEXT(LLVMRegionJIT) << "Compiling Region with " << rwu->block_count << " basic blocks.";

	if (rwu->block_count == 0)
		return false;

	LLVMContext ctx;
	Module *region_module = new Module("region", ctx);

	std::vector<Type *> params;
	params.push_back(IntegerType::getInt8PtrTy(ctx));
	params.push_back(IntegerType::getInt8PtrTy(ctx));

	FunctionType *fn = FunctionType::get(IntegerType::getInt32Ty(ctx), params, false);
	Function *region_fn = Function::Create(fn, GlobalValue::ExternalLinkage, "region", region_module);

	region_fn->addFnAttr(Attribute::NoRedZone);

	// Create some structural blocks
	BasicBlock *entry_block = BasicBlock::Create(ctx, "entry", region_fn);
	BasicBlock *dispatch_block = BasicBlock::Create(ctx, "dispatch", region_fn);
	BasicBlock *exit_block = BasicBlock::Create(ctx, "exit", region_fn);

	// Initialise the lowering context for this region.
	IRBuilder<> builder(ctx);

	LoweringContext lc(builder);
	lc.vtype = Type::getVoidTy(ctx);
	lc.i1 = IntegerType::getInt1Ty(ctx);
	lc.i8 = IntegerType::getInt8Ty(ctx); lc.pi8 = PointerType::get(lc.i8, 0);
	lc.i16 = IntegerType::getInt16Ty(ctx); lc.pi16 = PointerType::get(lc.i16, 0);
	lc.i32 = IntegerType::getInt32Ty(ctx); lc.pi32 = PointerType::get(lc.i32, 0);
	lc.i64 = IntegerType::getInt64Ty(ctx); lc.pi64 = PointerType::get(lc.i64, 0);

	// Implement the exit block
	builder.SetInsertPoint(exit_block);
	builder.CreateRet(lc.const32(1));

	// Fill in some useful values
	lc.region_fn = region_fn;
	lc.dispatch_block = dispatch_block;

	Function::ArgumentListType& args = region_fn->getArgumentList();
	lc.cpu_obj = &args.front();

	// Implement the entry block
	builder.SetInsertPoint(entry_block);

	// Initialise a pointer to the register state
	lc.reg_state = builder.CreatePtrToInt(args.getNext(&args.front()), lc.i64);

	// Initialise a pointer to the PC
	Value *pc_ptr_off = builder.CreateAdd(lc.reg_state, lc.const64(60));
	lc.pc_ptr = builder.CreateIntToPtr(pc_ptr_off, lc.pi32);
	set_aa_metadata(lc.pc_ptr, TAG_CLASS_REGISTER);

	// Initialise the virtual base address which this region was entered with
	lc.virtual_base_address = builder.CreateAnd(builder.CreateLoad(lc.pc_ptr), ~0xfffULL);
	builder.CreateBr(dispatch_block);

	// Now, create LLVM basic-blocks that represent each region basic-block.  We have
	// to do this as a separate pass, so that we can do optimised control-flow out
	// of one region basic-block, into another.
	for (uint32_t i = 0; i < rwu->block_count; i++) {
		lc.guest_block_entries[rwu->blocks[i].block_addr] = BasicBlock::Create(ctx, "gbb", region_fn);
	}

	// Loop over each region basic-block, and lower it to LLVM IR.
	for (uint32_t i = 0; i < rwu->block_count; i++) {
		if (!lower_block(lc, &rwu->blocks[i])) {
			return false;
		}
	}

	// Build the dispatcher
	builder.SetInsertPoint(dispatch_block);

	// Create a block that will actually perform the dispatch
	BasicBlock *do_dispatch_block = BasicBlock::Create(ctx, "do_dispatch", region_fn);

	// Load the virtual region-base of the PC, and compare it to the virtual base
	// that we entered with.
	Value *pc_to_dispatch = builder.CreateLoad(lc.pc_ptr);
	Value *pc_region = builder.CreateAnd(pc_to_dispatch, ~0xfffULL);

	// If the region bases are equal, then we can try and dispatch - we might have
	// a translation for that block.  Otherwise, if it's a different region, then
	// we must exit.  TODO: Implement region chaining
	builder.CreateCondBr(builder.CreateICmpNE(pc_region, lc.virtual_base_address), exit_block, do_dispatch_block);

	// Now, build the actual dispatcher
	builder.SetInsertPoint(do_dispatch_block);

	// Calculate the region-offset of the PC
	Value *pc_offset = builder.CreateAnd(pc_to_dispatch, 0xfff);

	// Create a switch statement to branch to the block.  TODO: Implement a jump-table
	SwitchInst *dispatcher = builder.CreateSwitch(pc_offset, exit_block);

	// Only add entry blocks into the switch statement.
	for (uint32_t i = 0; i < rwu->block_count; i++) {
		if (rwu->blocks[i].is_entry) {
			dispatcher->addCase(lc.const32(rwu->blocks[i].block_addr & 0xfff), lc.guest_block_entries[rwu->blocks[i].block_addr]);
		}
	}

	// Verify
	{
		PassManager verifyManager;
		verifyManager.add(createVerifierPass(true));
		verifyManager.run(*region_module);
	}

	// Print out the module
	{
		std::stringstream filename;
		filename << "region-" << std::hex << (uint64_t)(rwu->region_base_address) << ".ll";
		std::ofstream file(filename.str());
		raw_os_ostream str(file);

		PassManager printManager;
		printManager.add(createPrintModulePass(str, ""));
		printManager.run(*region_module);
	}

	// Optimise
	{
		PassManager optManager;
		initialise_pass_manager(&optManager);

		optManager.run(*region_module);
	}

	// Print out the module
	{
		std::stringstream filename;
		filename << "region-" << std::hex << (uint64_t)(rwu->region_base_address) << ".opt.ll";
		std::ofstream file(filename.str());
		raw_os_ostream str(file);

		PassManager printManager;
		printManager.add(createPrintModulePass(str, ""));
		printManager.run(*region_module);
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
		.setMCJITMemoryManager(std::unique_ptr<RTDyldMemoryManager>(new LLVMJITMemoryManager(_engine, *_shared_memory)))
		.setTargetOptions(target_opts)
		.create();

	if (!engine) {
		ERROR << CONTEXT(LLVMRegionJIT) << "Unable to create LLVM Engine";
		return false;
	}

	// We actually want compilation to happen.
	engine->DisableLazyCompilation();
	engine->finalizeObject();

	// Compile the function!
	void *ptr = (void *)engine->getFunctionAddress(region_fn->getName());
	if (!ptr) {
		ERROR << CONTEXT(LLVMRegionJIT) << "Unable to compile function";
		return false;
	}

	region_fn->deleteBody();
	delete engine;

	rwu->function_addr = (uint64_t)ptr;
	return true;
}

bool LLVMJIT::lower_block(LoweringContext& ctx, const shared::TranslationBlock* block)
{
	BlockLoweringContext block_ctx(ctx);

	// The alloca block is the start block for this region basic-block.
	block_ctx.alloca_block = ctx.guest_block_entries[block->block_addr];

	// Loop over the IR for this RBB and create LLVM basic-blocks to represent
	// each IR basic-block.
	for (uint32_t i = 0; i < block->ir_insn_count; i++) {
		if (block_ctx.ir_blocks.count(block->ir_insn[i].ir_block) == 0) {
			block_ctx.ir_blocks[block->ir_insn[i].ir_block] = BasicBlock::Create(ctx.builder.getContext(), "bb", ctx.region_fn);
		}
	}

	// There MUST be a block with ID zero - it's the entry block.
	assert(block_ctx.ir_blocks[0]);

	// Loop over the IR and lower the instruction.
	for (uint32_t i = 0; i < block->ir_insn_count; i++) {
		ctx.builder.SetInsertPoint(block_ctx.ir_blocks[block->ir_insn[i].ir_block]);
		if (!lower_ir_instruction(block_ctx, &block->ir_insn[i]))
			return false;
	}

	// At the end of the alloca block, create a branch to the entry IR block.
	ctx.builder.SetInsertPoint(block_ctx.alloca_block);
	ctx.builder.CreateBr(block_ctx.ir_blocks[0]);

	return false;
}
