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
				enum Status
				{
					NOT_IN_TRANSLATION,
					IN_TRANSLATION,
				};

				Region(Image& owner, gpa_t address);

				inline Image& owner() const { return _owner; }
				inline gpa_t address() const { return _address; }
				inline gpa_t offset() const { return _address & 0xfffULL; }

				Block& get_block(gpa_t gva);

				uint32_t hot_block_count();

				inline Status status() const { return _status; }
				inline void status(Status new_status) { _status = new_status; }

				inline util::map<gpa_t, Block *>::value_iterator begin() { return blocks.values_begin(); }
				inline util::map<gpa_t, Block *>::value_iterator end() { return blocks.values_end(); }

			private:
				Image& _owner;
				gpa_t _address;
				Status _status;

			public:
				util::map<gpa_t, Block *> blocks;
			};
		}
	}
}

#endif	/* REGION_H */

