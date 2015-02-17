/*
 * File:   mmu.h
 * Author: spink
 *
 * Created on 13 February 2015, 18:10
 */

#ifndef MMU_H
#define	MMU_H

#include <define.h>

#include "mm.h"

namespace captive {
	namespace arch {
		class Environment;

		class MMU
		{
		public:
			MMU(Environment& env);
			virtual ~MMU();

			virtual bool enable() = 0;
			virtual bool disable() = 0;
			virtual bool enabled() const = 0;

			inline Environment& env() const { return _env; }

			bool handle_fault(uint64_t va);

		private:
			Environment& _env;

		protected:
			bool clear_vma();
			void *map_guest_phys_page(gpa_t pa);
			void *map_guest_phys_pages(gpa_t pa, int nr);
			void unmap_phys_page(void *p);

			virtual bool resolve_gpa(gva_t va, gpa_t& pa) = 0;
		};
	}
}

#endif	/* MMU_H */

