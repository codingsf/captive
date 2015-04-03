/*
 * File:   decode.h
 * Author: spink
 *
 * Created on 27 February 2015, 08:20
 */

#ifndef DECODE_H
#define	DECODE_H

namespace captive {
	namespace arch {
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

