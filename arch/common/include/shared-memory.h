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
			struct shared_memory_block
			{
				uint32_t block_size;
			} packed;

			struct shared_memory_header
			{
				spinlock_t lock;
				void *next_free;
			} packed;

			void *_arena;
			uint64_t _arena_size;

			volatile struct shared_memory_header *_header;

			inline struct shared_memory_block *get_block(void *p) const
			{
				if (p) {
					return (struct shared_memory_block *)((uint64_t)p - sizeof(struct shared_memory_block));
				} else {
					return NULL;
				}
			}
		};
	}
}

#endif	/* SHARED_MEMORY_H */

