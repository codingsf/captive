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
			class Page;

			class Image
			{
			public:
				typedef std::map<gpa_t, Region *> region_map_t;
				typedef std::map<gpa_t, Page *> page_map_t;

				Image();
				~Image();

				Page& get_page(gpa_t gpa);

				Region& get_region(gpa_t gpa);
				Block& get_block(gpa_t gpa);

				void invalidate(gpa_t gpa);
				void invalidate();
				void invalidate_vaddr();

				region_map_t::iterator begin() { return regions.begin(); }
				region_map_t::iterator end() { return regions.end(); }

			private:
				region_map_t regions;
				Page *pages;
			};
		}
	}
}

#endif	/* IMAGE_H */

