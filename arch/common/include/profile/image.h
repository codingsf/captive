/*
 * File:   image.h
 * Author: spink
 *
 * Created on 16 March 2015, 17:47
 */

#ifndef IMAGE_H
#define	IMAGE_H

#include <define.h>
#include <map>

namespace captive {
	namespace arch {
		namespace profile {
			class Region;
			class Block;

			class Image
			{
			public:
				typedef std::map<gpa_t, Region *> region_map_t;

				Image();
				~Image();

				Region& get_region(gpa_t gpa);
				Block& get_block(gpa_t gpa);

				region_map_t::iterator begin() { return regions.begin(); }
				region_map_t::iterator end() { return regions.end(); }

			private:
				region_map_t regions;
			};
		}
	}
}

#endif	/* IMAGE_H */

