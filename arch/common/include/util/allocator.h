/*
 * File:   allocator.h
 * Author: spink
 *
 * Created on 16 March 2015, 18:15
 */

#ifndef ALLOCATOR_H
#define	ALLOCATOR_H

#include <malloc.h>

namespace captive {
	namespace arch {
		namespace util {
			template<typename T>
			class allocator
			{
			public:
				typedef size_t size_type;
				typedef T* pointer;
				typedef const T* const_pointer;
				typedef T& reference;
				typedef const T& const_reference;
				typedef T value_type;

				pointer allocate(size_type n)
				{
					return static_cast<pointer>(::operator new(n * sizeof(T)));
				}

				void deallocate(pointer p)
				{
					::operator delete(p);
				}
			};
		}
	}
}

#endif	/* ALLOCATOR_H */
