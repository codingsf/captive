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
				FETCH_FAULT,
				DEVICE_FAULT
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

			enum fault_reason {
				REASON_PAGE_INVALID,
				REASON_PERMISSIONS_FAIL
			};

			struct access_info {
				enum access_type type;
				enum access_mode mode;
				enum fault_reason reason;

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

			void set_page_executed(va_t va);
			void clear_page_executed(va_t va);
			bool is_page_executed(va_t va);
			bool clear_if_page_executed(va_t va);
			
			void set_page_device(va_t va);
			bool is_page_device(va_t va);

			void set_page_dirty(va_t va, bool dirty);
			bool is_page_dirty(va_t va);
			uint32_t page_checksum(va_t va);

			bool handle_fault(gva_t va, gpa_t& out_pa, const access_info& info, resolution_fault& fault);
			virtual bool resolve_gpa(gva_t va, gpa_t& pa, const access_info& info, resolution_fault& fault, bool have_side_effects = true) = 0;

			bool virt_to_phys(gva_t va, gpa_t& pa, resolution_fault& fault);

			void invalidate_virtual_mappings();
			void invalidate_virtual_mapping(gva_t va);

			void disable_writes();

		private:
			CPU& _cpu;

			inline pa_t gpa_to_hpa(gpa_t gpa) const {
				return (pa_t)(0x100000000ULL | (uint64_t)gpa);
			}

			inline gpa_t hpa_to_gpa(pa_t gpa) const {
				return (gpa_t)((uint64_t)gpa & 0xffffffffULL);
			}
		};
	}
}

#endif	/* MMU_H */

