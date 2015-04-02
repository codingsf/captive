/*
 * File:   region.h
 * Author: spink
 *
 * Created on 16 March 2015, 17:47
 */

#ifndef REGION_H
#define	REGION_H

#include <define.h>
#include <map>
#include <set>

namespace captive {
	namespace shared {
		struct RegionWorkUnit;
	}

	namespace arch {
		namespace profile {
			class Image;
			class Block;
			class Translation;

			class Region
			{
			public:
				enum Status
				{
					NOT_IN_TRANSLATION,
					IN_TRANSLATION,
				};

				typedef std::map<gpa_t, Block *> block_map_t;
				typedef std::set<gva_t> vaddr_set_t;

				Region(Image& owner, gpa_t address);

				inline Image& owner() const { return _owner; }
				inline gpa_t address() const { return _address; }
				inline gpa_t offset() const { return _address & 0xfffULL; }

				Block& get_block(gpa_t gpa);

				uint32_t hot_block_count();

				inline Status status() const { return _status; }
				inline void status(Status new_status) { _status = new_status; }

				inline block_map_t::iterator begin() { return blocks.begin(); }
				inline block_map_t::iterator end() { return blocks.end(); }

				inline uint32_t generation() const { return _generation; }

				void invalidate();
				void reset_heat();

				inline void add_virtual_base(gva_t address)
				{
					vaddrs.insert(address & ~0xfffULL);
				}

				const vaddr_set_t& virtual_bases() const { return vaddrs; }

				inline bool has_translation() const { return _txln != NULL; }
				inline void translation(Translation *txln) { _txln = txln; }
				inline Translation *translation() const { return _txln; }

				inline void set_work_unit(shared::RegionWorkUnit *rwu)
				{
					_rwu = rwu;
				}

			private:
				Image& _owner;
				gpa_t _address;
				Status _status;
				uint32_t _generation;
				Translation *_txln;
				shared::RegionWorkUnit *_rwu;

				block_map_t blocks;
				vaddr_set_t vaddrs;
			};
		}
	}
}

#endif	/* REGION_H */

