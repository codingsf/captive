#ifdef NDEBUG
#undef NDEBUG
#endif

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
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/InlineAsm.h>

#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/PassManager.h>

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/DependenceAnalysis.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Analysis/ScalarEvolution.h>

#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Vectorize.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include <shared-jit.h>

#include <map>
#include <vector>

#include <fstream>

USE_CONTEXT(JIT)
DECLARE_CHILD_CONTEXT(LLVM, JIT);

using namespace captive::jit;
using namespace llvm;

LLVMJIT::LLVMJIT(engine::Engine& engine, util::ThreadPool& worker_threads) : JIT(worker_threads), _engine(engine)
{
}

LLVMJIT::~LLVMJIT()
{
}

bool LLVMJIT::init()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetAsmPrinter();

	return true;
}

void *LLVMJIT::compile_region(uint32_t gpa, const std::vector<std::pair<uint32_t, shared::BlockTranslation *>>& blocks)
{
	RegionCompilationContext rcc;
	
	rcc.rgn_module = new Module("region", rcc.ctx);
	
	rcc.types.voidty = Type::getVoidTy(rcc.ctx);
	rcc.types.pi1 = (rcc.types.i1 = Type::getInt1Ty(rcc.ctx))->getPointerTo(0);
	rcc.types.pi8 = (rcc.types.i8 = Type::getInt8Ty(rcc.ctx))->getPointerTo(0);
	rcc.types.pi16 = (rcc.types.i16 = Type::getInt16Ty(rcc.ctx))->getPointerTo(0);
	rcc.types.pi32 = (rcc.types.i32 = Type::getInt32Ty(rcc.ctx))->getPointerTo(0);
	rcc.types.pi64 = (rcc.types.i64 = Type::getInt64Ty(rcc.ctx))->getPointerTo(0);
	
	FunctionType *rgn_func_ty = FunctionType::get(rcc.types.i32, { rcc.types.jit_state });
	rcc.rgn_func = Function::Create(rgn_func_ty, Function::ExternalLinkage, "region", rcc.rgn_module);
	
	rcc.entry_block = BasicBlock::Create(rcc.ctx, "entry", rcc.rgn_func);
	rcc.builder.SetInsertPoint(rcc.entry_block);
	rcc.builder.CreateRet(rcc.constu32(1));
	
	{
		PassManager verifyManager;
		verifyManager.add(createVerifierPass(true));
		verifyManager.run(*rcc.rgn_module);
	}
	
	// Initialise a new MCJIT engine
	TargetOptions target_opts;
	target_opts.DisableTailCalls = false;
	target_opts.PositionIndependentExecutable = true;
	target_opts.EnableFastISel = false;
	target_opts.NoFramePointerElim = true;
	target_opts.PrintMachineCode = false;

	ExecutionEngine *engine = EngineBuilder(std::unique_ptr<Module>(rcc.rgn_module))
		.setEngineKind(EngineKind::JIT)
		.setMCJITMemoryManager(std::unique_ptr<RTDyldMemoryManager>(new LLVMJITMemoryManager(_engine, *_shared_memory)))
		.setTargetOptions(target_opts)
		.create();

	if (!engine) {
		return NULL;
	}

	// We actually want compilation to happen.
	engine->DisableLazyCompilation();
	engine->finalizeObject();

	// Compile the function!
	void *ptr = (void *)engine->getFunctionAddress(rcc.rgn_func->getName());
	if (!ptr) {
		return NULL;
	}

	rcc.rgn_func->deleteBody();
	delete engine;

	return ptr;
}
