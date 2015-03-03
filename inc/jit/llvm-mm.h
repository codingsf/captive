/*
 * File:   llvm-mm.h
 * Author: spink
 *
 * Created on 03 March 2015, 08:21
 */

#ifndef LLVM_MM_H
#define	LLVM_MM_H

#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>

namespace captive {
	namespace jit {
		class LLVMJITMemoryManager : public llvm::RTDyldMemoryManager
		{
		public:
			LLVMJITMemoryManager(void *arena, uint64_t size);
			virtual ~LLVMJITMemoryManager();

			uint8_t* allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName) override;
			uint8_t* allocateDataSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName, bool IsReadOnly) override;

			bool finalizeMemory(std::string *ErrMsg) override;

			void *getPointerToNamedFunction(const std::string& Name, bool AbortOnFailure) override;

		private:
			void *arena, *next;
			uint64_t size;
		};
	}
}

#endif	/* LLVM_MM_H */

