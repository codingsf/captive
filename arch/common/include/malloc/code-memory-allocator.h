/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   code-memory-allocator.h
 * Author: s0457958
 *
 * Created on 19 August 2015, 11:11
 */

#ifndef CODE_MEMORY_ALLOCATOR_H
#define CODE_MEMORY_ALLOCATOR_H

#include <malloc/allocator.h>

namespace captive {
	namespace arch {
		namespace malloc {
			class CodeMemoryAllocator : public Allocator
			{
			public:
				void *alloc(size_t size) override;
				void *realloc(void *p, size_t new_size) override;
				void free(void *p) override;
			};
		}
	}
}

#endif /* CODE_MEMORY_ALLOCATOR_H */

