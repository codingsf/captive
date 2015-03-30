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
			enum alloc_flags_t {
				NONE = 0,
				ZERO = 1
			};

			SharedMemory(void *arena, uint64_t arena_size);

			void *allocate(size_t size, alloc_flags_t flags = NONE);
			void *reallocate(void *p, size_t size);
			void free(void *p);

		private:
			void *_arena;
			uint64_t _arena_size;

			// Shared Stuff
			struct shared_memory_block
			{
				struct shared_memory_block *next;
				uint64_t size;
				uint8_t data[];
			} packed;

			struct shared_memory_header
			{
				volatile spinlock_t lock;
				struct shared_memory_block *first;
			} packed;

			struct shared_memory_header *_header;
		};
	}
}

#endif	/* SHARED_MEMORY_H */

