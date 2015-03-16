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
				Block(Region& owner);

				inline Region& owner() const { return _owner; }

			private:
				Region& _owner;
			};
		}
	}
}

#endif	/* BLOCK_H */

