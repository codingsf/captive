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
				typedef bool (*GuestBasicBlockFn)(void *cpu, void *cpu_state);

				inline bool execute(void *cpu, void *cpu_state) {
					return _fnp(cpu, cpu_state);
				}

				inline void fnptr(GuestBasicBlockFn fnp) {
					_fnp = fnp;
				}

				inline GuestBasicBlockFn fnptr() const {
					return _fnp;
				}

				inline uint32_t block_address() const {
					return _block_address;
				}

				inline void block_address(uint32_t addr) {
					_block_address = addr;
				}

				inline void release_memory() { }

			private:
				uint32_t _block_address;
				GuestBasicBlockFn _fnp;
			};
		}
	}
}

#endif	/* GUEST_BASIC_BLOCK_H */

