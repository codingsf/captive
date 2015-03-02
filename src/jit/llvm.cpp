#include <jit/llvm.h>
#include <captive.h>

USE_CONTEXT(JIT)
DECLARE_CHILD_CONTEXT(LLVM, JIT);
DECLARE_CHILD_CONTEXT(LLVMBlockJIT, LLVM);

using namespace captive::jit;

LLVMJIT::~LLVMJIT()
{

}

bool LLVMJIT::init()
{
	return true;
}

uint64_t LLVMJIT::compile_block(const RawBytecode* bc, uint32_t count)
{
	DEBUG << CONTEXT(LLVMBlockJIT) << "Compiling";

	for (uint32_t idx = 0; idx < count; idx++) {
		bc[idx].dump();
	}

	return 0;
}

uint64_t LLVMJIT::compile_region(const RawBytecode* bc, uint32_t count)
{
	return 0;
}
