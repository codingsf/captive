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

	namespace jit {
		class LLVMJITMemoryManager : public llvm::RTDyldMemoryManager
		{
		public:
			LLVMJITMemoryManager(engine::Engine& engine, void *arena, uint64_t size);
			virtual ~LLVMJITMemoryManager();

			uint8_t* allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName) override;
			uint8_t* allocateDataSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName, bool IsReadOnly) override;

			bool finalizeMemory(std::string *ErrMsg) override;

			void *getPointerToNamedFunction(const std::string& Name, bool AbortOnFailure) override;

			uint64_t getSymbolAddress(const std::string& Name) override;

		private:
			engine::Engine& _engine;

			void *arena, *next;
			uint64_t size;

			struct Zone
			{
				Zone(void *base, uint64_t size) : base(base), size(size) { }

				void *base;
				uint64_t size;
			};

			std::list<Zone> free_zones;
			std::list<Zone> used_zones;
		};
	}
}

#endif	/* LLVM_MM_H */

