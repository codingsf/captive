/*
 * File:   malloc.h
 * Author: spink
 *
 * Created on 27 February 2015, 16:44
 */

#ifndef MALLOC_H
#define	MALLOC_H

#include <define.h>

extern "C" void *dlmalloc(size_t);
extern "C" void *dlrealloc(void *p, size_t);
extern "C" void *dlcalloc(size_t, size_t);
extern "C" void dlfree(void *p);

namespace captive {
	struct MemoryVector;

	namespace arch {
		void malloc_init(MemoryVector& arena);

		void *malloc(size_t size) { return dlmalloc(size); }

		void *realloc(void *p, size_t size) { return dlrealloc(p, size); }

		void *calloc(size_t nmemb, size_t size) { return dlcalloc(nmemb, size); }

		void free(void *p) { return dlfree(p); }
	}
}

#endif	/* MALLOC_H */

