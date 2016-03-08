/* 
 * File:   block.h
 * Author: s0457958
 *
 * Created on 06 July 2015, 16:18
 */

#ifndef BLOCK_H
#define	BLOCK_H

#include <define.h>
#include <malloc/malloc.h>
#include <shared-jit.h>

#include <set>

namespace captive {
	namespace arch {
		namespace profile {
			struct Block
			{
				Block() : txln(NULL) { }
				
				captive::shared::block_txln_fn txln;
				
				inline void invalidate()
				{
					if (txln) {
						// TODO: FIXME: XXX
						//malloc::data_alloc.free((void *)txln);
						txln = NULL;
					}
				}
			};
		}
	}
}

#endif	/* BLOCK_H */

