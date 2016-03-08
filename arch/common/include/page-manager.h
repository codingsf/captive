/* 
 * File:   page-manager.h
 * Author: s0457958
 *
 * Created on 08 March 2016, 09:43
 */

#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include <define.h>

namespace captive {
	namespace arch {
		struct Page;
		
		class PageManager
		{
		public:
			PageManager();
			~PageManager();
			
			Page *get_page(gpa_t pa) const;
			
		private:
			uint32_t _pages_buffer_page_count;
			Page *_pages;
		};
	}
}

#endif /* PAGE_MANAGER_H */

