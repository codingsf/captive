#ifndef TXLN_CACHE_H
#define TXLN_CACHE_H

#include <bitset>

template<typename inner_type_t, uint64_t cache_size> class cache
{
public:

	cache() {
		InvalidateAll();
	}

	inner_type_t *GetPtr() {
		return entries;
	}
	
	inner_type_t *GetEntryPtr(uint64_t entry_idx) {
		dirty_pages.set((entry_idx % cache_size) >> cache_bits);
		return &entries[entry_idx % cache_size];
	}
	
	void InvalidateAll() {
		for(uint64_t i = 0; i < cache_size; ++i) {
			GetEntryPtrClean(i)->invalidate();
		}
		dirty_pages.reset();
	}
	
	void InvalidateDirty() {
		if(dirty_pages.none()) return;
		
		for(uint32_t i = 0; i < dirty_pages.size(); ++i) {
			if(dirty_pages.test(i)) {
				for(uint64_t entry = i * cache_entry_count; entry < (i+1) * cache_entry_count; ++entry) {
					GetEntryPtrClean(entry)->invalidate();
				}
				
				dirty_pages.reset(i);
			}
		}

		assert(dirty_pages.none());
	}
	
	void InvalidateEntry(uint64_t entry_idx) {
		GetEntryPtr(entry_idx)->invalidate();
	}

private:
	static const uint32_t cache_bits = 12;
	static const uint32_t cache_page_count = cache_size >> cache_bits;
	static const uint32_t cache_entry_count = 1 << cache_bits;
	
	inner_type_t entries[cache_size];
	std::bitset<cache_page_count> dirty_pages;
	
	inner_type_t *GetEntryPtrClean(uint64_t entry_idx) {
		return &entries[entry_idx % cache_size];
	}
};

#endif
