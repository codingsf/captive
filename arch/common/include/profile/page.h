/*
 * File:   page.h
 * Author: spink
 *
 * Created on 10 June 2015, 15:29
 */

#ifndef PAGE_H
#define	PAGE_H

#include <define.h>
#include <set>

namespace captive {
	namespace arch {
		namespace profile {
			class Page
			{
			public:
				uint32_t base_address;

				inline bool has_entry(uint32_t address) const { return _entries.count(address & 0xfff); }
				inline void add_entry(uint32_t address) { _entries.insert(address & 0xfff); }

				inline int size() const { return _entries.size(); }
				inline void invalidate() { _entries.clear(); }

				inline const std::set<uint32_t>& entries() const { return _entries; }

				inline uint8_t *data() const { return (uint8_t *)(0x100000000 | base_address); }
			private:
				std::set<uint32_t> _entries;
			};
		}
	}
}

#endif	/* PAGE_H */

