/*
 * File:   local-memory.h
 * Author: spink
 *
 * Created on 27 April 2015, 13:09
 */

#ifndef LOCAL_MEMORY_H
#define	LOCAL_MEMORY_H

#include <malloc.h>

namespace captive {
	namespace arch {
		static inline void *lalloc(size_t size) { return malloc(size); }
		static inline void lfree(void* p) { free(p); }
		static inline void *lrealloc(void* p, size_t size) { return realloc(p, size); }
	}
}

#endif	/* LOCAL_MEMORY_H */
