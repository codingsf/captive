/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   data-memory-allocator.h
 * Author: s0457958
 *
 * Created on 19 August 2015, 11:10
 */

#ifndef DATA_MEMORY_ALLOCATOR_H
#define DATA_MEMORY_ALLOCATOR_H

#include <malloc/allocator.h>

namespace captive {
	namespace arch {
		namespace malloc {
			class DataMemoryAllocator : public Allocator
			{
			public:
				void *alloc(size_t size) override;
				void *realloc(void *p, size_t new_size) override;
				void free(void *p) override;
			};
		}
	}
}

#endif /* DATA_MEMORY_ALLOCATOR_H */

