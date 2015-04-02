/*
 * File:   jit.h
 * Author: spink
 *
 * Created on 02 March 2015, 08:49
 */

#ifndef JIT_H
#define	JIT_H

#include <define.h>

namespace captive {
	namespace hypervisor {
		class SharedMemory;
	}

	namespace util {
		class ThreadPool;
	}

	namespace shared {
		struct RegionWorkUnit;
		struct BlockWorkUnit;
		struct TranslationBlock;
	}

	namespace jit {
		class BlockJIT;
		class RegionJIT;

		class JIT
		{
		public:
			JIT(util::ThreadPool& worker_threads);
			virtual ~JIT();

			inline util::ThreadPool& worker_threads() const { return _worker_threads; }

			virtual bool init() = 0;

			virtual BlockJIT& block_jit() = 0;
			virtual RegionJIT& region_jit() = 0;

			void set_shared_memory(hypervisor::SharedMemory& shmem) {
				_shared_memory = &shmem;
			}

		protected:
			util::ThreadPool& _worker_threads;
			hypervisor::SharedMemory *_shared_memory;

			bool quick_opt(shared::TranslationBlock *tb);
		};

		class JITStrategy
		{
		public:
			JITStrategy(JIT& owner);
			virtual ~JITStrategy();

			inline JIT& owner() const { return _owner; }

		private:
			JIT& _owner;
		};

		class BlockJIT : public JITStrategy
		{
		public:
			typedef void (*block_completion_t)(shared::BlockWorkUnit *work_unit, bool success, void *completion_data);

			BlockJIT(JIT& owner);
			virtual ~BlockJIT();
			virtual bool compile_block(shared::BlockWorkUnit *work_unit) = 0;
			void compile_block_async(shared::BlockWorkUnit *work_unit, block_completion_t completion, void *completion_data);
		};

		class RegionJIT : public JITStrategy
		{
		public:
			typedef void (*region_completion_t)(shared::RegionWorkUnit *work_unit, bool success, void *completion_data);

			RegionJIT(JIT& owner);
			virtual ~RegionJIT();

			virtual bool compile_region(shared::RegionWorkUnit *work_unit) = 0;
			void compile_region_async(shared::RegionWorkUnit *work_unit, region_completion_t completion, void *completion_data);
		};
	}
}

#endif	/* JIT_H */

