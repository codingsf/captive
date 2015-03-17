/*
 * File:   image.h
 * Author: spink
 *
 * Created on 16 March 2015, 17:47
 */

#ifndef IMAGE_H
#define	IMAGE_H

#include <define.h>
#include <util/map.h>

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

				Region& get_region(gpa_t gpa);
				Block& get_block(gpa_t gpa);

				//util::map<gpa_t, Region *>::value_iterator begin() { return regions.values_begin(); }
				//util::map<gpa_t, Region *>::value_iterator end() { return regions.values_end(); }

			//private:
				util::map<gpa_t, Region *> regions;
			};
		}
	}
}

#endif	/* IMAGE_H */

