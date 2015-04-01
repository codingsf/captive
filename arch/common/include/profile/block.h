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
			class Translation;

			class Block
			{
			public:
				Block(Region& owner, gpa_t address);

				inline Region& owner() const { return _owner; }
				inline gpa_t address() const { return _address;}

				inline void inc_interp_count() { _interp_count++; }
				inline void reset_interp_count() { _interp_count = 0; }
				inline uint32_t interp_count() const { return _interp_count; }

				inline Translation *translation() const { return _txln; }
				inline void translation(Translation *txln) { _txln = txln; }
				inline bool has_translation() const { return _txln != NULL; }

				inline void invalidate() { _txln = NULL; _interp_count = 0; }

			private:
				Region& _owner;
				gpa_t _address;
				uint32_t _interp_count;
				Translation *_txln;
			};
		}
	}
}

#endif	/* BLOCK_H */

