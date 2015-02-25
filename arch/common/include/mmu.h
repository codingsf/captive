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
		class CPU;

		class MMU
		{
		public:
			enum resolution_fault {
				NONE,
				READ_FAULT,
				WRITE_FAULT,
				FETCH_FAULT
			};

			enum access_type {
				ACCESS_READ,
				ACCESS_WRITE,
				ACCESS_FETCH,
			};

			enum access_mode {
				ACCESS_USER,
				ACCESS_KERNEL,
			};

			struct access_info {
				enum access_type type;
				enum access_mode mode;
			};

			MMU(CPU& cpu);
			virtual ~MMU();

			virtual bool enable() = 0;
			virtual bool disable() = 0;
			virtual bool enabled() const = 0;

			inline CPU& cpu() const { return _cpu; }

			bool handle_fault(va_t va, resolution_fault& fault);

			inline void flush() {
				clear_vma();
			}

		private:
			CPU& _cpu;

		protected:
			bool clear_vma();
			void *map_guest_phys_page(gpa_t pa);
			void *map_guest_phys_pages(gpa_t pa, int nr);
			void unmap_phys_page(void *p);

			virtual bool resolve_gpa(gva_t va, gpa_t& pa, const access_info& info, resolution_fault& fault) = 0;
		};
	}
}

#endif	/* MMU_H */

