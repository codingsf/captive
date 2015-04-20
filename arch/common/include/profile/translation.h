/*
 * File:   region-translation.h
 * Author: spink
 *
 * Created on 01 April 2015, 12:08
 */

#ifndef REGION_TRANSLATION_H
#define	REGION_TRANSLATION_H

#include <define.h>
#include <cpu.h>
#include <mm.h>
#include <shared-memory.h>

namespace captive {
	namespace arch {
		namespace profile {
			class Translation
			{
			public:
				typedef uint32_t (*translation_fn_t)(void *jit_state);

				Translation(translation_fn_t fn) : _fn(fn) { }
				~Translation() { Memory::shared_memory().free((void *)_fn); }

				inline uint32_t execute(CPU& cpu) { return _fn(&cpu.jit_state); }

				inline void *fn_ptr() const { return (void *)_fn; }

			private:
				translation_fn_t _fn;
			};
		}
	}
}

#endif	/* REGION_TRANSLATION_H */

