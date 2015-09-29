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
				Block() : exec_count(0), entry(false), loop_header(false), txln(NULL), ir(NULL), ir_count(0) { }
				
				uint32_t exec_count;
				bool entry, loop_header;
				captive::shared::block_txln_fn txln;
				const captive::shared::IRInstruction *ir;
				uint32_t ir_count;
				
				inline void invalidate()
				{
					exec_count = 0;
					entry = false;
					loop_header = false;
					
					if (txln) {
						// TODO: FIXME: XXX
						//malloc::data_alloc.free((void *)txln);
						txln = NULL;
					}
					
					if (ir) {
						malloc::data_alloc.free((void *)ir);
						ir = NULL;
					}
				}
			};
		}
	}
}

#endif	/* BLOCK_H */

