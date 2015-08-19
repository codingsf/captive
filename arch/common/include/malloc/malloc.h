/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   maloc.h
 * Author: s0457958
 *
 * Created on 19 August 2015, 11:13
 */

#ifndef MALOC_H
#define MALOC_H

#include <malloc/code-memory-allocator.h>
#include <malloc/data-memory-allocator.h>
#include <malloc/shared-memory-allocator.h>

namespace captive {
	namespace arch {
		namespace malloc {
			extern DataMemoryAllocator data_alloc;
			extern CodeMemoryAllocator code_alloc;
			extern SharedMemoryAllocator shmem_alloc;
		}
	}
}

#endif /* MALOC_H */
