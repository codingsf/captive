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

				inline void invalidate()
				{
					_block_address = 0xffffffff;
					_fnp = NULL;
					_fnp_offset = 0;
				}

				inline void update(uint32_t addr, GuestBasicBlockFn fnp, uint64_t off)
				{
					_block_address = addr;
					_fnp = fnp;
					_fnp_offset = off;
				}

				inline GuestBasicBlockFn fnptr() const {
					return _fnp;
				}

				inline uint32_t block_address() const {
					return _block_address;
				}

				inline uint64_t fnp_offset() const {
					return _fnp_offset;
				}

				inline bool valid() const {
					return _block_address != 0xffffffff;
				}

				inline void release_memory()
				{
					asm volatile("out %0, $0xff" :: "a"((uint8_t)7), "D"(_fnp_offset));
				}

			private:
				uint32_t _block_address;
				GuestBasicBlockFn _fnp;
				uint64_t _fnp_offset;
			} packed;

			static_assert(sizeof(GuestBasicBlock) == 20, "GuestBasicBlock incorrect size");
		}
	}
}

#endif	/* GUEST_BASIC_BLOCK_H */

