#include <jit/llvm.h>
#include <jit/llvm-mm.h>
#include <shared-jit.h>
#include <captive.h>

USE_CONTEXT(JIT)
USE_CONTEXT(LLVM)
DECLARE_CHILD_CONTEXT(LLVMPageJIT, LLVM);

using namespace captive::jit;
using namespace captive::shared;
using namespace llvm;

bool LLVMJIT::compile_page(PageWorkUnit *pwu)
{
	return false;
}
