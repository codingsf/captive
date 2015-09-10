/*
 * File:   decode.h
 * Author: spink
 *
 * Created on 27 February 2015, 08:20
 */

#ifndef DECODE_H
#define	DECODE_H

#include <define.h>

namespace captive {
	namespace arch {
		#define UNSIGNED_BITS(v, u, l)	(((uint32_t)(v) << (31 - u)) >> (31 - u + l))
		#define SIGNED_BITS(v, u, l)	(((int32_t)(v) << (31 - u)) >> (31 - u + l))
		#define BITSEL(v, b)			(((v) >> b) & 1UL)
		#define BIT_LSB(i)				(1 << (i))
		
		class Decode
		{
		public:
			uint32_t pc;
			uint32_t length;

			bool end_of_block;
			bool is_predicated;
		};

		class JumpInfo
		{
		public:
			enum JumpType
			{
				NONE,
				DIRECT,
				INDIRECT
			};

			JumpType type;
			uint32_t target;
		};
	}
}

#endif	/* DECODE_H */

