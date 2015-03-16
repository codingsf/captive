/*
 * File:   region.h
 * Author: spink
 *
 * Created on 16 March 2015, 17:47
 */

#ifndef REGION_H
#define	REGION_H

#include <define.h>

namespace captive {
	namespace arch {
		namespace profile {
			class Image;
			class Block;

			class Region
			{
			public:
				Region(Image& owner);

				inline Image& owner() const { return _owner; }

				Block& get_block(gva_t gva);

			private:
				Image& _owner;
			};
		}
	}
}

#endif	/* REGION_H */

