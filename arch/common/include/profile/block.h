/*
 * File:   block.h
 * Author: spink
 *
 * Created on 16 March 2015, 17:47
 */

#ifndef BLOCK_H
#define	BLOCK_H

#include <define.h>

namespace captive {
	namespace arch {
		namespace profile {
			class Region;

			class Block
			{
			public:
				typedef uint32_t (*block_fn_t)(void *, void *);

				Block(Region& owner, gpa_t address);

				inline Region& owner() const { return _owner; }
				inline gpa_t address() const { return _address;}

				inline void inc_interp_count() { _interp_count++; }
				inline void reset_interp_count() { _interp_count = 0; }
				inline uint32_t interp_count() const { return _interp_count; }

				inline void set_translation(block_fn_t fn) { _block_fn = fn; }
				inline bool have_translation() const { return _block_fn != NULL; }
				inline uint32_t execute(void *a, void *b) { return _block_fn(a, b); }

				inline void invalidate() { _block_fn = NULL; }

			private:
				Region& _owner;
				gpa_t _address;
				uint32_t _interp_count;
				block_fn_t _block_fn;
			};
		}
	}
}

#endif	/* BLOCK_H */

