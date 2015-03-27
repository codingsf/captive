/*
 * File:   shared-memory.h
 * Author: spink
 *
 * Created on 26 March 2015, 18:14
 */

#ifndef SHARED_MEMORY_H
#define	SHARED_MEMORY_H

#include <define.h>
#include <lock.h>

namespace captive {
	namespace arch {
		class SharedMemory
		{
		public:
			SharedMemory(void *arena, uint64_t arena_size);

			void *allocate(size_t size);
			void free(void *p);

		private:
			struct shared_memory_header
			{
				spinlock_t lock;
				void *next_free;
			} packed;

			void *_arena;
			uint64_t _arena_size;

			volatile struct shared_memory_header *_header;
		};
	}
}

#endif	/* SHARED_MEMORY_H */

