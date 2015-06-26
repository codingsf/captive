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

void LLVMJIT::compile_region_async(uint32_t gpa, const std::vector<uint32_t>& blocks)
{
	fprintf(stderr, "compile region %08x\n", gpa);
}
