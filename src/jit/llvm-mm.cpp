#include <jit/llvm-mm.h>
#include <hypervisor/shared-memory.h>
#include <engine/engine.h>
#include <captive.h>

USE_CONTEXT(LLVM);

using namespace captive::jit;

LLVMJITMemoryManager::LLVMJITMemoryManager(engine::Engine& engine, hypervisor::SharedMemory& shared_memory) : _engine(engine), _shared_memory(shared_memory)
{
	DEBUG << CONTEXT(LLVM) << "Creating LLVMJITMemoryManager";
}

LLVMJITMemoryManager::~LLVMJITMemoryManager()
{
	DEBUG << CONTEXT(LLVM) << "Destroying LLVMJITMemoryManager";
}

uint8_t* LLVMJITMemoryManager::allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName)
{
	void *region = _shared_memory.allocate((size_t)Size);

	if (!region) {
		return NULL;
	} else {
		_allocated_memory.push_back(region);
		return (uint8_t *)region;
	}
}

uint8_t* LLVMJITMemoryManager::allocateDataSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName, bool IsReadOnly)
{
	void *region = _shared_memory.allocate((size_t)Size);

	if (!region) {
		return NULL;
	} else {
		_allocated_memory.push_back(region);
		return (uint8_t *)region;
	}
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
	uint64_t value;
	if (!_engine.lookup_symbol(Name, value))
		return 0;
	return value;
}
