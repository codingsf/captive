/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   page.h
 * Author: s0457958
 *
 * Created on 08 March 2016, 09:42
 */

#ifndef PAGE_H
#define PAGE_H

#include <define.h>

namespace captive {
	namespace arch {
		struct Page {
			uintptr_t phys_addr;
			uint64_t flags;
			uint64_t pad0, pad1;
			
			inline void *data() const { return vm_phys_to_virt(phys_addr); }
		} packed;
	}
}

#endif /* PAGE_H */

