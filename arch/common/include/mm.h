/*
 * File:   mm.h
 * Author: spink
 *
 * Created on 12 February 2015, 14:31
 */

#ifndef MM_H
#define	MM_H

#include <define.h>

namespace captive {
	namespace arch {
		class Memory {
		public:
			Memory(uint64_t first_phys_page);

		private:
			uint64_t _first_phys_page;
		};
	}
}

#endif	/* MM_H */

