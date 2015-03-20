#include <jit/llvm.h>
#include <captive.h>

USE_CONTEXT(JIT)
USE_CONTEXT(LLVM)
DECLARE_CHILD_CONTEXT(LLVMRegionJIT, LLVM);

using namespace captive::jit;

void *LLVMJIT::internal_compile_region(const RawBlockDescriptors* bd, const RawBytecodeDescriptor* bcd)
{
	DEBUG << "Compiling Region";

	return 0;
}

