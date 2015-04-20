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

	// Initialise the lowering context for this region.
	IRBuilder<> builder(ctx);

	LoweringContext lc(builder, *rwu);
	lc.vtype = Type::getVoidTy(ctx);
	lc.i1 = IntegerType::getInt1Ty(ctx);
	lc.i8 = IntegerType::getInt8Ty(ctx); lc.pi8 = PointerType::get(lc.i8, 0);
	lc.i16 = IntegerType::getInt16Ty(ctx); lc.pi16 = PointerType::get(lc.i16, 0);
	lc.i32 = IntegerType::getInt32Ty(ctx); lc.pi32 = PointerType::get(lc.i32, 0);
	lc.i64 = IntegerType::getInt64Ty(ctx); lc.pi64 = PointerType::get(lc.i64, 0);

	std::vector<Type *> jit_state_elements;
	jit_state_elements.push_back(lc.pi8);
	jit_state_elements.push_back(lc.pi8);
	jit_state_elements.push_back(lc.i32);
	jit_state_elements.push_back(lc.pi8->getPointerTo(0));

	Type *jit_state_type = StructType::get(ctx, jit_state_elements);
	PointerType *jit_state_ptr_type = jit_state_type->getPointerTo(0);

	std::vector<Type *> params;
	params.push_back(jit_state_ptr_type);

	FunctionType *fn = FunctionType::get(lc.i32, params, false);
	Function *region_fn = Function::Create(fn, GlobalValue::ExternalLinkage, "region", region_module);

	region_fn->addFnAttr(Attribute::NoRedZone);

	Function::ArgumentListType& args = region_fn->getArgumentList();
	Value *jit_state_ptr_val = &args.front();

	// Create some structural blocks
	BasicBlock *entry_block = BasicBlock::Create(ctx, "entry", region_fn);
	BasicBlock *dispatch_block = BasicBlock::Create(ctx, "dispatch", region_fn);
	BasicBlock *chain_block = BasicBlock::Create(ctx, "chain", region_fn);
	BasicBlock *exit_block = BasicBlock::Create(ctx, "exit", region_fn);

	// Implement the exit block
	builder.SetInsertPoint(exit_block);
	builder.CreateRet(lc.const32(1));

	// Fill in some useful values
	lc.region_fn = region_fn;
	lc.dispatch_block = dispatch_block;
	lc.chain_block = chain_block;
	lc.exit_block = exit_block;

	// Implement the entry block
	builder.SetInsertPoint(entry_block);

	// Initialise a pointer to the register state
	Value *jit_state_val = builder.CreateLoad(jit_state_ptr_val);
	lc.cpu_obj = builder.CreateExtractValue(jit_state_val, {0}, "cpu_object");
	lc.reg_state = builder.CreatePtrToInt(builder.CreateExtractValue(jit_state_val, {1}, "reg_state"), lc.i64);
	lc.region_chain_tbl = builder.CreateExtractValue(jit_state_val, {3}, "chain_tbl");

	// Initialise a pointer to the PC
	Value *pc_ptr_off = builder.CreateAdd(lc.reg_state, lc.const64(60));
	lc.pc_ptr = builder.CreateIntToPtr(pc_ptr_off, lc.pi32);
	set_aa_metadata(lc.pc_ptr, TAG_CLASS_REGISTER, lc.const64(60));

	// Initialise the virtual base address which this region was entered with
	lc.virtual_base_address = builder.CreateAnd(builder.CreateLoad(lc.pc_ptr), ~0xfffULL);
	builder.CreateBr(dispatch_block);

	// Now, create LLVM basic-blocks that represent each region basic-block.  We have
	// to do this as a separate pass, so that we can do optimised control-flow out
	// of one region basic-block, into another.
	for (uint32_t i = 0; i < rwu->block_count; i++) {
		DEBUG << CONTEXT(LLVMRegionJIT) << "Creating LLVM BB for Region BB 0x" << std::hex << rwu->blocks[i].block_addr;

		std::stringstream rbb_name;
		rbb_name << "rbb-" << std::hex << rwu->blocks[i].block_addr;
		lc.guest_block_entries[rwu->blocks[i].block_addr] = BasicBlock::Create(ctx, rbb_name.str(), region_fn);
	}

	// Loop over each region basic-block, and lower it to LLVM IR.
	for (uint32_t i = 0; i < rwu->block_count; i++) {
		DEBUG << CONTEXT(LLVMRegionJIT) << "Lowering Region BB " << std::hex << rwu->blocks[i].block_addr;
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
	// we can try and chain.
	builder.CreateCondBr(builder.CreateICmpEQ(pc_region, lc.virtual_base_address), do_dispatch_block, chain_block);

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

	// Build the chainer
	builder.SetInsertPoint(chain_block);
	Value *chain_destination = builder.CreateAnd(builder.CreateLoad(lc.pc_ptr), ~0xfffULL);
	Value *chain_slot = builder.CreateGEP(lc.region_chain_tbl, { chain_destination });
	Value *chain_dest = builder.CreateLoad(chain_slot);

	BasicBlock *do_chain_block = BasicBlock::Create(ctx, "do_chain", region_fn);
	builder.CreateCondBr(builder.CreateICmpEQ(chain_dest, lc.nullptr8()), exit_block, do_chain_block);

	builder.SetInsertPoint(do_chain_block);
	builder.CreateCall()->setTailCall(true);

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
		//print_module(filename.str(), region_module);
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
		print_module(filename.str(), region_module);
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
	BlockLoweringContext block_ctx(ctx, *block);

	// The alloca block is the start block for this region basic-block.
	block_ctx.alloca_block = ctx.guest_block_entries[block->block_addr];

	// Do quick IR optimisations
	//quick_opt(block);

	// Loop over the IR for this RBB and create LLVM basic-blocks to represent
	// each IR basic-block.
	for (uint32_t i = 0; i < block->ir_insn_count; i++) {
		if (block_ctx.ir_blocks.count(block->ir_insn[i].ir_block) == 0) {
			DEBUG << CONTEXT(LLVMRegionJIT) << "Inserting LLVM BB for IR BB " << block->ir_insn[i].ir_block;

			std::stringstream irb_name;
			irb_name << "irb-" << std::hex << block->block_addr << "-" << block->ir_insn[i].ir_block;
			block_ctx.ir_blocks[block->ir_insn[i].ir_block] = BasicBlock::Create(ctx.builder.getContext(), irb_name.str(), ctx.region_fn);
		}
	}

	// There MUST be a block with ID zero - it's the entry block.
	assert(block_ctx.ir_blocks[0]);

	// Loop over the IR and lower the instruction.
	for (uint32_t i = 0; i < block->ir_insn_count; i++) {
		DEBUG << CONTEXT(LLVMRegionJIT) << "Lowering IR Instruction from IR BB " << block->ir_insn[i].ir_block << ": " << InstructionPrinter::render_instruction(block->ir_insn[i]);

		ctx.builder.SetInsertPoint(block_ctx.ir_blocks[block->ir_insn[i].ir_block]);
		if (!lower_ir_instruction(block_ctx, &block->ir_insn[i]))
			return false;
	}

	// At the end of the alloca block, create a branch to the entry IR block.
	ctx.builder.SetInsertPoint(block_ctx.alloca_block);
	ctx.builder.CreateBr(block_ctx.ir_blocks[0]);

	return true;
}
