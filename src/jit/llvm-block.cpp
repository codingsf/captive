#include <jit/llvm.h>
#include <jit/llvm-mm.h>
#include <jit/allocator.h>
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

#include <map>
#include <vector>

USE_CONTEXT(LLVM);
DECLARE_CHILD_CONTEXT(LLVMBlockJIT, LLVM);

using namespace captive::jit;
using namespace llvm;

void *LLVMJIT::internal_compile_block(const RawBytecodeDescriptor* bcd)
{
	if (bcd->bytecode_count == 0)
		return NULL;

	LLVMContext ctx;
	Module *block_module = new Module("block", ctx);

	std::vector<Type *> params;
	params.push_back(IntegerType::getInt8PtrTy(ctx));
	params.push_back(IntegerType::getInt8PtrTy(ctx));

	FunctionType *fn = FunctionType::get(IntegerType::getInt32Ty(ctx), params, false);
	Function *block_fn = Function::Create(fn, GlobalValue::ExternalLinkage, "block", block_module);

	block_fn->addFnAttr(Attribute::NoRedZone);

	BasicBlock *entry_block = BasicBlock::Create(ctx, "entry", block_fn);

	IRBuilder<> builder(ctx);
	builder.SetInsertPoint(entry_block);

	LoweringContext lc(builder);
	lc.vtype = Type::getVoidTy(ctx);
	lc.i1 = IntegerType::getInt1Ty(ctx);
	lc.i8 = IntegerType::getInt8Ty(ctx); lc.pi8 = PointerType::get(lc.i8, 0);
	lc.i16 = IntegerType::getInt16Ty(ctx); lc.pi16 = PointerType::get(lc.i16, 0);
	lc.i32 = IntegerType::getInt32Ty(ctx); lc.pi32 = PointerType::get(lc.i32, 0);
	lc.i64 = IntegerType::getInt64Ty(ctx); lc.pi64 = PointerType::get(lc.i64, 0);

	Function::ArgumentListType& args = block_fn->getArgumentList();

	lc.cpu_obj = &args.front();
	lc.reg_state = builder.CreatePtrToInt(args.getNext(&args.front()), lc.i64);

	// Populate the basic-block map
	for (uint32_t idx = 0; idx < bcd->bytecode_count; idx++) {
		if (lc.basic_blocks[bcd->bc[idx].block_id] == NULL) {
			lc.basic_blocks[bcd->bc[idx].block_id] = BasicBlock::Create(ctx, "bb", block_fn);
		}
	}

	lc.alloca_block = entry_block;

	// Lower all instructions
	for (uint32_t idx = 0; idx < bcd->bytecode_count; idx++) {
		const RawBytecode *bc = &bcd->bc[idx];
		BasicBlock *bb = lc.basic_blocks[bc->block_id];

		builder.SetInsertPoint(bb);
		if (!lower_bytecode(lc, bc)) {
			ERROR << CONTEXT(LLVMBlockJIT) << "Failed to lower byte-code: " << bc->render();
			return NULL;
		}
	}

	// Create a branch to the first block, from the entry block.
	builder.SetInsertPoint(entry_block);
	assert(lc.basic_blocks[0]);
	builder.CreateBr(lc.basic_blocks[0]);

	// Verify
	{
		PassManager verifyManager;
		verifyManager.add(createVerifierPass(true));
		verifyManager.run(*block_module);

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

		optManager.run(*block_module);
	}

	// Print out the module
	/*{
		raw_os_ostream str(std::cerr);

		PassManager printManager;
		printManager.add(createPrintModulePass(str, ""));
		printManager.run(*block_module);
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

	ExecutionEngine *engine = EngineBuilder(std::unique_ptr<Module>(block_module))
		.setEngineKind(EngineKind::JIT)
		/*.setOptLevel(CodeGenOpt::Aggressive)
		.setRelocationModel(Reloc::PIC_)*/
		//.setUseMCJIT(true)
		.setMCJITMemoryManager(std::unique_ptr<RTDyldMemoryManager>(new LLVMJITMemoryManager(_engine, *_allocator)))
		.setTargetOptions(target_opts)
		.create();

	if (!engine) {
		ERROR << CONTEXT(LLVMBlockJIT) << "Unable to create LLVM Engine";
		return NULL;
	}

	// We actually want compilation to happen.
	engine->DisableLazyCompilation();
	engine->finalizeObject();

	// Compile the function!
	void *ptr = (void *)engine->getFunctionAddress(block_fn->getName());
	if (!ptr) {
		ERROR << CONTEXT(LLVMBlockJIT) << "Unable to compile function";
		return NULL;
	}

	block_fn->deleteBody();
	delete engine;

	return ptr;
}

