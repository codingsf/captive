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
	
			enum permissions {
				NO_PERMISSION   = 0,
				USER_READ    = 0x01,
				USER_WRITE   = 0x02,
				USER_FETCH   = 0x04,
				KERNEL_READ	 = 0x10,
				KERNEL_WRITE = 0x20,
				KERNEL_FETCH = 0x40,
				
				USER_READ_WRITE   = 0x03,
				USER_READ_FETCH   = 0x05,
				USER_ALL		  = 0x07,
				
				KERNEL_READ_WRITE = 0x30,
				KERNEL_READ_FETCH = 0x50,
				KERNEL_ALL = 0x70,
				
				ALL_READ_FETCH    = 0x55,
				ALL_READ_WRITE	  = 0x33,
				ALL_READ_WRITE_FETCH = 0x77,
			};
			
			struct resolution_context
			{
				explicit resolution_context(gva_t va) : va(va) { }
				
				const gva_t va;
				gpa_t pa;
								
				enum permissions requested_permissions;
				enum permissions allowed_permissions;
				
				enum resolution_fault fault;
				
				bool emulate_user;
				
				inline bool is_write() const { return !!(requested_permissions & (USER_WRITE | KERNEL_WRITE)); }
				
				/*inline bool is_read() const { return type == ACCESS_READ; }
				inline bool is_write() const { return type == ACCESS_WRITE; }
				inline bool is_fetch() const { return type == ACCESS_FETCH; }
				inline bool is_kernel() const { return mode == ACCESS_KERNEL; }
				inline bool is_user() const { return mode == ACCESS_USER; }*/
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

			bool handle_fault(struct resolution_context& context);
			virtual bool resolve_gpa(struct resolution_context& context, bool have_side_effects = true) = 0;

			bool translate_fetch(struct resolution_context& context);

			void invalidate_virtual_mappings();
			void invalidate_virtual_mapping(gva_t va);
			void invalidate_virtual_mapping_by_context_id(uint32_t context_id);

			void disable_writes();
			
			inline uint32_t context_id() const { return _context_id; }
			void context_id(uint32_t ctxid);

		protected:
			inline bool permit_access(const struct resolution_context& context) const
			{
				return !!(context.requested_permissions & context.allowed_permissions);
			}
			
		private:
			CPU& _cpu;
			uint32_t _context_id;
		};
	}
}

#endif	/* MMU_H */

