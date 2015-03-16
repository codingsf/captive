/*
 * File:   image.h
 * Author: spink
 *
 * Created on 16 March 2015, 17:47
 */

#ifndef IMAGE_H
#define	IMAGE_H

#include <define.h>

namespace captive {
	namespace arch {
		namespace profile {
			class Region;
			class Block;

			class Image
			{
			public:
				Image();
				~Image();

				Region& get_region(gva_t gva);
				Block& get_block(gva_t gva);
			};
		}
	}
}

#endif	/* IMAGE_H */

