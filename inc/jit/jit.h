/*
 * File:   jit.h
 * Author: spink
 *
 * Created on 02 March 2015, 08:49
 */

#ifndef JIT_H
#define	JIT_H

#include <define.h>

namespace captive {
	namespace jit {
		class JIT
		{
		public:
			bool init();
			uint64_t compile_block(void *ir, uint32_t count);
		};
	}
}

#endif	/* JIT_H */

