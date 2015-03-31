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

			inline void *allocate(size_t size, alloc_flags_t flags = NONE)
			{
				uint64_t addr;
				asm volatile ("out %2, $0xff" : "=a"(addr) : "D"(size), "a"(10));
				return (void *)addr;
			}

			inline void *reallocate(void *p, size_t size)
			{
				uint64_t addr;
				asm volatile ("out %3, $0xff" : "=a"(addr) : "D"(p), "S"(size), "a"(11));
				return (void *)addr;
			}

			inline void free(void *p)
			{
				asm volatile ("out %1, $0xff" : : "D"(p), "a"(12));
			}
		};
	}
}

#endif	/* SHARED_MEMORY_H */

