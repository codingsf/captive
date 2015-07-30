/* 
 * File:   region.h
 * Author: s0457958
 *
 * Created on 06 July 2015, 16:18
 */

#ifndef REGION_H
#define	REGION_H

#include <define.h>
#include <string.h>
#include <profile/block.h>

namespace captive {
	namespace shared {
		struct RegionWorkUnit;
	}
	
	namespace arch {
		namespace profile {
			struct Block;
			
			struct Region
			{
				Region() : txln(NULL), rwu(NULL) { bzero(blocks, sizeof(blocks)); }
				
				Block *blocks[0x1000];
				captive::shared::region_txln_fn txln;
				shared::RegionWorkUnit *rwu;
				uint32_t heat;
				
				inline Block *get_block(uint32_t addr)
				{
					Block **block_ptr = &blocks[addr & 0xfff];
					if (*block_ptr == NULL) {
						*block_ptr = new Block();
					}
					
					return *block_ptr;
				}
				
				inline void invalidate()
				{
					if (rwu) {
						rwu->valid = 0;
					}

					if (txln) {
						shfree((void *)txln);
						txln = NULL;
					}
					
					for (int i = 0; i < 0x1000; i++) {
						if (blocks[i]) {
							blocks[i]->invalidate();
						}
					}
				}
			};
		}
	}
}

#endif	/* REGION_H */

