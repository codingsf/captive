/*
 * File:   guest-basic-block.h
 * Author: spink
 *
 * Created on 27 February 2015, 12:49
 */

#ifndef GUEST_BASIC_BLOCK_H
#define	GUEST_BASIC_BLOCK_H

#include <define.h>

namespace captive {
	namespace arch {
		class JIT;

		namespace jit {
			class GuestBasicBlock
			{
			public:
				typedef bool (*GuestBasicBlockFn)(void *cpu_state);

				inline bool execute(void *cpu_state) const {
					return fnp(cpu_state);
				}

				inline void fnptr(GuestBasicBlockFn fnp) {
					fnp = fnp;
				}

				inline uint32_t block_address() const {
					return _block_address;
				}

				inline void block_address(uint32_t addr) {
					_block_address = addr;
				}

			private:
				uint32_t _block_address;
				GuestBasicBlockFn fnp;
			};
		}
	}
}

#endif	/* GUEST_BASIC_BLOCK_H */

