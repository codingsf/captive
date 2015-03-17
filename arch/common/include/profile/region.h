/*
 * File:   region.h
 * Author: spink
 *
 * Created on 16 March 2015, 17:47
 */

#ifndef REGION_H
#define	REGION_H

#include <define.h>
#include <util/map.h>

namespace captive {
	namespace arch {
		namespace profile {
			class Image;
			class Block;

			class Region
			{
			public:
				Region(Image& owner, gpa_t address);

				inline Image& owner() const { return _owner; }
				inline gpa_t address() const { return _address; }

				Block& get_block(gpa_t gva);

				uint32_t hot_block_count();

			private:
				Image& _owner;
				gpa_t _address;

				util::map<gpa_t, Block *> blocks;
			};
		}
	}
}

#endif	/* REGION_H */

