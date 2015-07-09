/* 
 * File:   image.h
 * Author: s0457958
 *
 * Created on 06 July 2015, 16:18
 */

#ifndef IMAGE_H
#define	IMAGE_H

#include <define.h>
#include <string.h>
#include <profile/region.h>

namespace captive {
	namespace arch {
		namespace profile {
			struct Region;
			struct Block;
			
			struct Image
			{
				Image() { bzero(regions, sizeof(regions)); }
				
				Region *regions[0x100000];
				
				inline Region *get_region(uint32_t addr)
				{
					Region **region_ptr = &regions[addr >> 12];
					if (*region_ptr == NULL) *region_ptr = new Region();
					
					return *region_ptr;
				}
				
				inline Region *get_region_from_index(uint32_t idx)
				{
					Region **region_ptr = &regions[idx];
					if (*region_ptr == NULL) *region_ptr = new Region();
					
					return *region_ptr;
				}
				
				inline void invalidate()
				{
					for (int i = 0; i < 0x100000; i++) {
						if (regions[i])
							regions[i]->invalidate();
					}
				}
			};
		}
	}
}

#endif	/* IMAGE_H */

