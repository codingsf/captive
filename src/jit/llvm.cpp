#include <jit/llvm.h>
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
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>

#include <map>
#include <vector>

USE_CONTEXT(JIT)
DECLARE_CHILD_CONTEXT(LLVM, JIT);
DECLARE_CHILD_CONTEXT(LLVMBlockJIT, LLVM);

using namespace captive::jit;
using namespace llvm;

LLVMJIT::LLVMJIT() : mm(NULL)
{
}

LLVMJIT::~LLVMJIT()
{
	if (mm) {
		delete mm;
	}
}

bool LLVMJIT::init()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetAsmPrinter();

	return true;
}

void *LLVMJIT::compile_block(const RawBytecode* bc, uint32_t count)
{
	if (count == 0)
		return NULL;

	LLVMContext ctx;
	Module block_module("block", ctx);

	std::vector<Type *> params;
	params.push_back(IntegerType::getInt8PtrTy(ctx));

	FunctionType *fn = FunctionType::get(IntegerType::getInt32Ty(ctx), params, false);
	Function *block_fn = Function::Create(fn, GlobalValue::ExternalLinkage, "block", &block_module);
	BasicBlock *entry_block = BasicBlock::Create(ctx, "entry", block_fn);

	IRBuilder<> builder(ctx);

	std::map<uint32_t, BasicBlock *> blocks;
	for (uint32_t idx = 0; idx < count; idx++) {
		BasicBlock *bb = blocks[bc[idx].block_id];
		if (!bb) {
			bb = BasicBlock::Create(ctx, "bb", block_fn);
			blocks[bc[idx].block_id] = bb;
		}

		builder.SetInsertPoint(bb);
		LowerBytecode(builder, &bc[idx]);
	}

	assert(blocks[0]);

	builder.SetInsertPoint(entry_block);
	builder.CreateBr(blocks[0]);

	{
		raw_os_ostream str(std::cerr);

		PassManager printManager;
		printManager.add(createPrintModulePass(str, ""));
		printManager.run(block_module);
	}

	if (!mm) {
		mm = new LLVMJITMemoryManager(_code_arena, _code_arena_size);
	}

	TargetOptions target_opts;
	target_opts.DisableTailCalls = false;
	target_opts.PositionIndependentExecutable = true;
	target_opts.EnableFastISel = false;
	target_opts.NoFramePointerElim = true;
	target_opts.PrintMachineCode = false;

	ExecutionEngine *engine = EngineBuilder(&block_module)
		.setUseMCJIT(true)
		.setMCJITMemoryManager(mm)
		.setTargetOptions(target_opts)
		.setAllocateGVsWithCode(true)
		.create();

	if (!engine) {
		ERROR << CONTEXT(LLVMBlockJIT) << "Unable to create LLVM Engine";
		return NULL;
	}

	void *ptr = (void *)engine->getFunctionAddress("block");
	if (!ptr) {
		ERROR << CONTEXT(LLVMBlockJIT) << "Unable to compile function";
		return NULL;
	}

	DEBUG << CONTEXT(LLVMBlockJIT) << "Compiled function to " << std::hex << (uint64_t)ptr << ", X=" << (uint32_t)*(uint8_t *)ptr;

	return ptr;
}

void *LLVMJIT::compile_region(const RawBytecode* bc, uint32_t count)
{
	return 0;
}

bool LLVMJIT::LowerBytecode(llvm::IRBuilder<>& builder, const RawBytecode* bc)
{
	DEBUG << CONTEXT(LLVMBlockJIT) << "Lowering: " << bc->render();
	if (builder.GetInsertBlock()->size() == 0)
		builder.CreateRet(ConstantInt::get(IntegerType::getInt32Ty(builder.getContext()), 1));
	return true;
}
