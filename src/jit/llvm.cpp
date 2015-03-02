#include <jit/llvm.h>

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
	return 0;
}

uint64_t LLVMJIT::compile_region(const RawBytecode* bc, uint32_t count)
{
	return 0;
}
