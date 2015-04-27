/*
 * File:   local-memory.h
 * Author: spink
 *
 * Created on 27 April 2015, 13:09
 */

#ifndef LOCAL_MEMORY_H
#define	LOCAL_MEMORY_H

#include <allocator.h>
#include <malloc.h>

namespace captive {
	namespace arch {
		class LocalMemory : public Allocator
		{
		public:
			virtual void* allocate(size_t size, alloc_flags_t flags = NONE) { return malloc(size); }
			virtual void free(void* p) { return free(p); }
			virtual void* reallocate(void* p, size_t size) { return realloc(p, size); }
		};
	}
}

#endif	/* LOCAL_MEMORY_H */

