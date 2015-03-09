#include <jit/llvm-mm.h>
#include <engine/engine.h>
#include <captive.h>

USE_CONTEXT(LLVM);

using namespace captive::jit;

LLVMJITMemoryManager::LLVMJITMemoryManager(engine::Engine& engine, void *arena, uint64_t size) : _engine(engine), arena(arena), size(size)
{
	free_zones.push_back(Zone((void *)((uint8_t *)arena + 0x1000), size));
}

LLVMJITMemoryManager::~LLVMJITMemoryManager()
{

}

uint8_t* LLVMJITMemoryManager::allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName)
{
	if (free_zones.size() == 0)
		return NULL;

	Zone free_zone = free_zones.front();
	free_zones.pop_front();

	uint64_t zone_size = Size;

	if (zone_size % 4096) {
		zone_size += 4096 - (zone_size % 4096);
	}

	Zone used_zone = Zone(free_zone.base, zone_size);

	free_zone.size -= used_zone.size;

	if (free_zone.size < 0) {
		return NULL;
	} else if (free_zone.size > 0) {
		free_zone.base = (void *)(((uint8_t *)used_zone.base) + used_zone.size);
		free_zones.push_back(free_zone);
	}

	DEBUG << CONTEXT(LLVM) << "Allocated Zone: base=" << std::hex << (uint64_t)used_zone.base << ", size=" << used_zone.size;

	used_zones.push_back(used_zone);
	return (uint8_t *)used_zone.base;
}

uint8_t* LLVMJITMemoryManager::allocateDataSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName, bool IsReadOnly)
{
	return allocateCodeSection(Size, Alignment, SectionID, SectionName);
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
	//DEBUG << CONTEXT(LLVM) << "Attempting to resolve function: " << Name;

	uint64_t value;
	if (!_engine.lookup_symbol(Name, value))
		return 0;
	return value;
}
