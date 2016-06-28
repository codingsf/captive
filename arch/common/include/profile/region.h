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
#include <malloc/allocator.h>
#include <shared-jit.h>

namespace captive {
	namespace shared {
		struct RegionWorkUnit;
	}

	namespace arch {
		namespace profile {
			struct Region
			{
				enum region_addr_flags
				{
					RGN_ADDR_NONE = 0,
					RGN_ADDR_START = 1,
					RGN_ADDR_CODE = 2,
					RGN_ADDR_END = 3
				};
				
				Region() { invalidate(); }

				captive::shared::block_txln_fn txlns[0x1000];
				
				inline captive::shared::block_txln_fn get_txln(uint32_t addr) const
				{
					return txlns[addr & 0xfff];
				}
				
				inline void set_txln(uint32_t addr, captive::shared::block_txln_fn txln)
				{
					txlns[addr & 0xfff] = txln;
				}
				
				/*inline void set_addr(uint32_t addr, region_addr_flags flags)
				{
					uint16_t bit = (addr & 0xfff) << 1;
					
					uint8_t mask = 3 << (bit % 8);
					uint8_t val = (flags & 3) << (bit % 8);
					
					bitmap[bit / 8] &= ~mask;
					bitmap[bit / 8] |= val;
				}*/

				inline void invalidate()
				{
					bzero(txlns, sizeof(txlns));
				}
			};
		}
	}
}

#endif	/* REGION_H */
