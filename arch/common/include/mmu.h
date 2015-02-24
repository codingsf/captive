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
			enum resolution_fault {
				NONE,
				FAULT,
			};

			MMU(Environment& env);
			virtual ~MMU();

			virtual bool enable() = 0;
			virtual bool disable() = 0;
			virtual bool enabled() const = 0;

			inline Environment& env() const { return _env; }

			bool handle_fault(va_t va, resolution_fault& fault);

			inline void flush() {
				clear_vma();
			}

		private:
			Environment& _env;

		protected:
			bool clear_vma();
			void *map_guest_phys_page(gpa_t pa);
			void *map_guest_phys_pages(gpa_t pa, int nr);
			void unmap_phys_page(void *p);

			enum access_type {
				READ,
				WRITE,
				FETCH,
			};

			virtual bool resolve_gpa(gva_t va, gpa_t& pa, access_type type, resolution_fault& fault) = 0;
		};
	}
}

#endif	/* MMU_H */

