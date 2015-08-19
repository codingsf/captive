/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   page-allocator.h
 * Author: s0457958
 *
 * Created on 19 August 2015, 11:15
 */

#ifndef PAGE_ALLOCATOR_H
#define PAGE_ALLOCATOR_H

#include <define.h>

namespace captive {
	namespace arch {
		namespace malloc {
			class PageAllocator
			{
			public:
				PageAllocator();
				
				void init(void *heap, size_t heap_size);
				void *alloc_pages(int nr_pages);
				void free_pages(void *p);
				
			private:
				void *_heap;
				size_t _heap_size;
				
				void *_next_page;
			};
			
			extern PageAllocator page_alloc;
		}
	}
}

#endif /* PAGE_ALLOCATOR_H */

