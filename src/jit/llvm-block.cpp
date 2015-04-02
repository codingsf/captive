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

#include <map>
#include <vector>

USE_CONTEXT(LLVM);
DECLARE_CHILD_CONTEXT(LLVMBlockJIT, LLVM);

using namespace captive::jit;
using namespace captive::shared;
using namespace llvm;

bool LLVMJIT::compile_block(BlockWorkUnit *bwu)
{
	return false;
}
