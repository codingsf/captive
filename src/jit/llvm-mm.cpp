#include <jit/llvm-mm.h>

using namespace captive::jit;

LLVMJITMemoryManager::LLVMJITMemoryManager(void *arena, uint64_t size) : arena(arena), size(size)
{

}

LLVMJITMemoryManager::~LLVMJITMemoryManager()
{

}

uint8_t* LLVMJITMemoryManager::allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName)
{
	return (uint8_t *)arena;
}

uint8_t* LLVMJITMemoryManager::allocateDataSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName, bool IsReadOnly)
{
	return (uint8_t *)arena + 0x100000;
}

bool LLVMJITMemoryManager::finalizeMemory(std::string* ErrMsg)
{
	return true;
}
