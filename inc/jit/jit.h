/*
 * File:   jit.h
 * Author: spink
 *
 * Created on 02 March 2015, 08:49
 */

#ifndef JIT_H
#define	JIT_H

#include <define.h>
#include <vector>

namespace captive {
	namespace hypervisor {
		class SharedMemory;
		class CPU;
	}

	namespace util {
		class ThreadPool;
	}

	namespace shared {
		struct IRInstruction;
		struct IROperand;
		struct BlockTranslation;
		struct RegionWorkUnit;
	}

	namespace jit {
		class JIT
		{
		public:
			JIT(util::ThreadPool& worker_threads);
			virtual ~JIT();

			virtual bool init() = 0;

			void analyse(hypervisor::CPU& cpu, shared::RegionWorkUnit *rwu);
			
			inline util::ThreadPool& worker_threads() const { return _worker_threads; }

			inline void set_shared_memory(hypervisor::SharedMemory& shmem) {
				_shared_memory = &shmem;
			}
			
			virtual void *compile_region(shared::RegionWorkUnit *rwu) = 0;

		protected:
			util::ThreadPool& _worker_threads;
			hypervisor::SharedMemory *_shared_memory;
		};

		class InstructionPrinter
		{
		public:
			static std::string render_instruction(const shared::IRInstruction& insn);
			static std::string render_operand(const shared::IROperand& oper);
		};
		
		class NullJIT : public JIT
		{
		public:
			NullJIT();
			virtual ~NullJIT();
			
			bool init() override;
			void *compile_region(shared::RegionWorkUnit *rwu) override;
		};
	}
}

#endif	/* JIT_H */
