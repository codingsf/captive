/*
 * File:   mm.h
 * Author: spink
 *
 * Created on 12 February 2015, 14:31
 */

#ifndef MM_H
#define	MM_H

#include <define.h>

#define packed __attribute__((packed))

namespace captive {
	namespace arch {
		union entry_t {
			uint64_t data;
			struct {
				uint64_t addr : 52;
				uint32_t flags : 12;
			};
		} packed;

		typedef entry_t page_map_entry_t;
		typedef entry_t page_desc_ptr_entry_t;
		typedef entry_t page_desc_entry_t;
		typedef entry_t page_table_entry_t;

		struct page_map_t {
			page_map_entry_t entries[4];
		} packed;

		struct page_desc_ptr_t {
			page_desc_ptr_entry_t entries[512];
		} packed;

		struct page_desc_t {
			page_desc_entry_t entries[512];
		} packed;

		struct page_table_t {
			page_table_entry_t entries[512];
		} packed;

		class Memory {
		public:
			Memory(uint64_t first_phys_page);

			static void *alloc_page();

		private:
			static Memory *mm;
			
			uint64_t _first_phys_page;
			void *_data_base;
		};
	}
}

#endif	/* MM_H */

