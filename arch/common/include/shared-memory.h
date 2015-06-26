/*
 * File:   shared-memory.h
 * Author: spink
 *
 * Created on 26 March 2015, 18:14
 */

#ifndef SHARED_MEMORY_H
#define	SHARED_MEMORY_H

#include <define.h>
#include <string.h>
#include <lock.h>
#include <allocator.h>

namespace captive {
	namespace arch {
		class SharedMemory : public Allocator
		{
		public:
			SharedMemory() { }

			virtual void *allocate(size_t size, alloc_flags_t flags = NONE) override
			{
				uint64_t addr;
				asm volatile ("out %2, $0xff" : "=a"(addr) : "D"(size), "a"(10));

				if (addr && (flags & ZERO) == ZERO) {
					bzero((void *)addr, size);
				}

				return (void *)addr;
			}

			virtual void *reallocate(void *p, size_t size) override
			{
				uint64_t addr;
				asm volatile ("out %3, $0xff" : "=a"(addr) : "D"(p), "S"(size), "a"(11));
				return (void *)addr;
			}

			virtual void free(void *p) override
			{
				asm volatile ("out %1, $0xff" : : "D"(p), "a"(12));
			}
		};
	}
}

#endif	/* SHARED_MEMORY_H */

