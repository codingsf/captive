/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   malloc.h
 * Author: s0457958
 *
 * Created on 19 August 2015, 11:09
 */

#ifndef MALLOC_H
#define MALLOC_H

#include <define.h>

namespace captive {
	namespace arch {
		namespace malloc {
			class Allocator
			{
			public:
				virtual void *alloc(size_t size) = 0;
				virtual void *calloc(size_t nmemb, size_t elemsize);
				virtual void *realloc(void *p, size_t new_size) = 0;
				virtual void free(void *p) = 0;
			};
		}
	}
}

#endif /* MALLOC_H */
