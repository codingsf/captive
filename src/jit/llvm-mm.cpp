#include <jit/llvm-mm.h>
#include <engine/engine.h>
#include <captive.h>

using namespace captive::jit;

LLVMJITMemoryManager::LLVMJITMemoryManager(engine::Engine& engine, void *arena, uint64_t size) : _engine(engine), arena(arena), size(size)
{

}

LLVMJITMemoryManager::~LLVMJITMemoryManager()
{

}

uint8_t* LLVMJITMemoryManager::allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName)
{
	return (uint8_t *)arena + 0x1000;
}

uint8_t* LLVMJITMemoryManager::allocateDataSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName, bool IsReadOnly)
{
	return (uint8_t *)arena + 0x101000;
}

bool LLVMJITMemoryManager::finalizeMemory(std::string* ErrMsg)
{
	return true;
}

void* LLVMJITMemoryManager::getPointerToNamedFunction(const std::string& Name, bool AbortOnFailure)
{
	return NULL;
}

uint64_t LLVMJITMemoryManager::getSymbolAddress(const std::string& Name)
{
	DEBUG << "Attempting to resolve function: " << Name;

	uint64_t value;
	if (!_engine.lookup_symbol(Name, value))
		return 0;
	return value;
}
