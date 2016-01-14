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
				DEVICE_FAULT,
				SMC_FAULT
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
			
			struct resolve_request
			{
				gva_t va;
				enum access_type type;
				enum access_mode mode;
				bool emulate_user;
				
				inline bool is_read() const { return type == ACCESS_READ; }
				inline bool is_write() const { return type == ACCESS_WRITE; }
				inline bool is_fetch() const { return type == ACCESS_FETCH; }
				inline bool is_kernel() const { return mode == ACCESS_KERNEL; }
				inline bool is_user() const { return mode == ACCESS_USER; }
			};
			
			struct resolve_response
			{
				gpa_t pa;
				resolution_fault fault;
			};

			MMU(CPU& cpu);
			virtual ~MMU();

			virtual bool enable() = 0;
			virtual bool disable() = 0;
			virtual bool enabled() const = 0;

			inline CPU& cpu() const { return _cpu; }

			void set_page_executed(hva_t va);
			void clear_page_executed(hva_t va);
			bool is_page_executed(hva_t va);
			bool clear_if_page_executed(hva_t va);
			
			void set_page_device(hva_t va);
			bool is_page_device(hva_t va);

			void set_page_dirty(hva_t va, bool dirty);
			bool is_page_dirty(hva_t va);
			uint32_t page_checksum(hva_t va);

			bool handle_fault(const struct resolve_request& request, struct resolve_response& response);
			virtual bool resolve_gpa(const struct resolve_request& request, struct resolve_response& response, bool have_side_effects = true) = 0;

			bool translate_fetch(gva_t va, struct resolve_response& response);

			void invalidate_virtual_mappings();
			void invalidate_virtual_mapping(gva_t va);

			void disable_writes();

		private:
			CPU& _cpu;
		};
	}
}

#endif	/* MMU_H */

