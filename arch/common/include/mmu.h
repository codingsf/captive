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

				inline bool is_read() const { return type == ACCESS_READ; }
				inline bool is_write() const { return type == ACCESS_WRITE; }
				inline bool is_fetch() const { return type == ACCESS_FETCH; }
				inline bool is_kernel() const { return mode == ACCESS_KERNEL; }
				inline bool is_user() const { return mode == ACCESS_USER; }
			};

			MMU(CPU& cpu);
			virtual ~MMU();

			virtual bool enable() = 0;
			virtual bool disable() = 0;
			virtual bool enabled() const = 0;

			inline CPU& cpu() const { return _cpu; }

			void cpu_privilege_change(bool kernel_mode);
			void set_page_executed(uint32_t va);

			bool handle_fault(gva_t va, const access_info& info, resolution_fault& fault);
			virtual bool resolve_gpa(gva_t va, gpa_t& pa, const access_info& info, resolution_fault& fault, bool have_side_effects = true) = 0;

			inline void flush() {
				clear_vma();
			}

			inline void invalidate(gva_t va)
			{
				page_map_entry_t *pm;
				page_dir_ptr_entry_t *pdp;
				page_dir_entry_t *pd;
				page_table_entry_t *pt;

				Memory::get_va_table_entries((va_t)(uint64_t)va, pm, pdp, pd, pt);
				pt->present(false);

				Memory::flush_page((va_t)(uint64_t)va);
			}

		private:
			CPU& _cpu;

			inline pa_t gpa_to_hpa(gpa_t gpa) const {
				return (pa_t)(0x100000000ULL | (uint64_t)gpa);
			}

		protected:
			bool clear_vma();
		};
	}
}

#endif	/* MMU_H */

