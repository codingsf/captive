/*
 * File:   llvm-mm.h
 * Author: spink
 *
 * Created on 03 March 2015, 08:21
 */

#ifndef LLVM_MM_H
#define	LLVM_MM_H

#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <list>

namespace captive {
	namespace engine {
		class Engine;
	}

	namespace hypervisor {
		class SharedMemory;
	}

	namespace jit {
		class Allocator;
		class AllocationRegion;

		class LLVMJITMemoryManager : public llvm::RTDyldMemoryManager
		{
		public:
			LLVMJITMemoryManager(engine::Engine& engine, hypervisor::SharedMemory& shared_memory);
			virtual ~LLVMJITMemoryManager();

			uint8_t* allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName) override;
			uint8_t* allocateDataSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName, bool IsReadOnly) override;

			bool finalizeMemory(std::string *ErrMsg) override;

			void *getPointerToNamedFunction(const std::string& Name, bool AbortOnFailure) override;

			uint64_t getSymbolAddress(const std::string& Name) override;

		private:
			engine::Engine& _engine;
			hypervisor::SharedMemory& _shared_memory;

			std::list<void *> _allocated_memory;
		};
	}
}

#endif	/* LLVM_MM_H */

