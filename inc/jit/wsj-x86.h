/*
 * File:   wsj-x86.h
 * Author: spink
 *
 * Created on 10 March 2015, 16:51
 */

#ifndef WSJ_X86_H
#define	WSJ_X86_H

#include <define.h>

namespace captive {
	namespace jit {
		namespace x86 {
			class X86Builder
			{
			public:
				X86Builder();

				bool generate(void *buffer, uint64_t& size);
			};
		}
	}
}

#endif	/* WSJ_X86_H */

