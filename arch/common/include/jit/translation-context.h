/*
 * File:   translation-context.h
 * Author: spink
 *
 * Created on 27 February 2015, 15:07
 */

#ifndef TRANSLATION_CONTEXT_H
#define	TRANSLATION_CONTEXT_H

#include <define.h>
#include <jit/guest-basic-block.h>

namespace captive {
	namespace arch {
		namespace jit {
			class TranslationContext
			{
			public:
				GuestBasicBlock::GuestBasicBlockFn compile();
			};
		}
	}
}

#endif	/* TRANSLATION_CONTEXT_H */

