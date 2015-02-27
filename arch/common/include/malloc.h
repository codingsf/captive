/*
 * File:   malloc.h
 * Author: spink
 *
 * Created on 27 February 2015, 16:44
 */

#ifndef MALLOC_H
#define	MALLOC_H

#include <define.h>

namespace captive {
	namespace arch {
		void *malloc(size_t size);
		void *realloc(void *p, size_t size);
		void free(void *p);
	}
}

#endif	/* MALLOC_H */

