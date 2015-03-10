#include <jit/llvm-mm.h>
#include <jit/allocator.h>
#include <engine/engine.h>
#include <captive.h>

USE_CONTEXT(LLVM);

using namespace captive::jit;

LLVMJITMemoryManager::LLVMJITMemoryManager(engine::Engine& engine, Allocator& allocator) : _engine(engine), _allocator(allocator)
{
	DEBUG << CONTEXT(LLVM) << "Creating LLVMJITMemoryManager";
}

LLVMJITMemoryManager::~LLVMJITMemoryManager()
{
	DEBUG << CONTEXT(LLVM) << "Destroying LLVMJITMemoryManager";
}

uint8_t* LLVMJITMemoryManager::allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName)
{
	AllocationRegion *region = _allocator.allocate(Size);

	if (!region) {
		return NULL;
	} else {
		_regions.push_back(region);
		return (uint8_t *)region->base_address();
	}
}

uint8_t* LLVMJITMemoryManager::allocateDataSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName, bool IsReadOnly)
{
	AllocationRegion *region = _allocator.allocate(Size);

	if (!region) {
		return NULL;
	} else {
		_regions.push_back(region);
		return (uint8_t *)region->base_address();
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
