/*
 * File:   allocator.h
 * Author: spink
 *
 * Created on 27 April 2015, 13:06
 */

#ifndef ALLOCATOR_H
#define	ALLOCATOR_H

namespace captive {
	namespace arch {
		class Allocator
		{
		public:
			enum alloc_flags_t {
				NONE = 0,
				ZERO = 1
			};

			virtual void *allocate(size_t size, alloc_flags_t flags = NONE) = 0;
			virtual void *reallocate(void *p, size_t size) = 0;
			virtual void free(void *p) = 0;
		};
	}
}

#endif	/* ALLOCATOR_H */

