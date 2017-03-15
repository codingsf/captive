/*
 * File:   mm.h
 * Author: spink
 *
 * Created on 12 February 2015, 14:31
 */

#ifndef MM_H
#define	MM_H

#include <define.h>
#include <printf.h>

#include <priv.h>

#define packed __attribute__((packed))

namespace captive {
	namespace arch {
		class MMU;

		class Memory {
			friend class MMU;

		public:
			static void init();
			
		private:
			Memory();
			
		public:
			/*static inline uint32_t get_va_flags(hva_t va)
			{
				page_map_entry_t* pm;
				page_dir_ptr_entry_t* pdp;
				page_dir_entry_t* pd;
				page_table_entry_t* pt;

				get_va_table_entries(va, pm, pdp, pd, pt);
				return pt->flags();
			}

			static inline void set_va_flags(hva_t va, uint32_t flags)
			{
				page_map_entry_t* pm;
				page_dir_ptr_entry_t* pdp;
				page_dir_entry_t* pd;
				page_table_entry_t* pt;

				get_va_table_entries(va, pm, pdp, pd, pt);
				pt->flags(flags);

				flush_page(va);
			}

			static inline void prevent_writes(hva_t addr) {
				page_map_entry_t* pm;
				page_dir_ptr_entry_t* pdp;
				page_dir_entry_t* pd;
				page_table_entry_t* pt;

				get_va_table_entries(addr, pm, pdp, pd, pt);
				
				pt->writable(false);
				flush_page(addr);
			}
			
			static inline bool quick_txl(hva_t va, hpa_t& pa)
			{
				table_idx_t pm_idx = 0, pdp_idx, pd_idx, pt_idx;
				va_table_indicies(va, pm_idx, pdp_idx, pd_idx, pt_idx);

				page_map_entry_t* pm;
				pm = &((page_map_t *)vm_phys_to_virt(CR3))->entries[pm_idx];
				if (!pm->present()) {
					return false;
				}

				page_dir_ptr_entry_t* pdp;
				pdp = &((page_dir_ptr_t *)vm_phys_to_virt(pm->base_address()))->entries[pdp_idx];
				if (!pdp->present()) {
					return false;
				}

				page_dir_entry_t* pd;
				pd = &((page_dir_t *)vm_phys_to_virt(pdp->base_address()))->entries[pd_idx];
				if (!pd->present()) {
					return false;
				}

				page_table_entry_t* pt = &((page_table_t *)vm_phys_to_virt(pd->base_address()))->entries[pt_idx];
				if (!pt->present()) {
					return false;
				}
								
				pa = pt->base_address();
				return true;
			}
			
			static inline bool quick_fetch(hva_t va, hpa_t& pa, bool user_mode)
			{
				// TODO: FIXME: 
				if (va >= 0x100000000) return false;
				
				table_idx_t pm_idx = 0, pdp_idx, pd_idx, pt_idx;
				va_table_indicies(va, pm_idx, pdp_idx, pd_idx, pt_idx);

				page_map_entry_t* pm;
				pm = &((page_map_t *)vm_phys_to_virt(CR3))->entries[pm_idx];
				if (!pm->present()) {
					return false;
				}

				page_dir_ptr_entry_t* pdp;
				pdp = &((page_dir_ptr_t *)vm_phys_to_virt(pm->base_address()))->entries[pdp_idx];
				if (!pdp->present()) {
					return false;
				}

				page_dir_entry_t* pd;
				pd = &((page_dir_t *)vm_phys_to_virt(pdp->base_address()))->entries[pd_idx];
				if (!pd->present()) {
					return false;
				}

				page_table_entry_t* pt = &((page_table_t *)vm_phys_to_virt(pd->base_address()))->entries[pt_idx];
				if (!pt->present() || !pt->executable() || (!pt->allow_user() && user_mode)) {
					return false;
				}
								
				pa = (pt->base_address() | (va & 0xfff));
				return true;
			}
			
			static inline bool quick_read(hva_t va, hpa_t& pa, bool user_mode)
			{
				table_idx_t pm_idx = 0, pdp_idx, pd_idx, pt_idx;
				va_table_indicies(va, pm_idx, pdp_idx, pd_idx, pt_idx);

				page_map_entry_t* pm;
				pm = &((page_map_t *)vm_phys_to_virt(CR3))->entries[pm_idx];
				if (!pm->present()) {
					return false;
				}

				page_dir_ptr_entry_t* pdp;
				pdp = &((page_dir_ptr_t *)vm_phys_to_virt(pm->base_address()))->entries[pdp_idx];
				if (!pdp->present()) {
					return false;
				}

				page_dir_entry_t* pd;
				pd = &((page_dir_t *)vm_phys_to_virt(pdp->base_address()))->entries[pd_idx];
				if (!pd->present()) {
					return false;
				}

				page_table_entry_t* pt = &((page_table_t *)vm_phys_to_virt(pd->base_address()))->entries[pt_idx];
				if (!pt->present() || (!pt->allow_user() && user_mode)) {
					return false;
				}
								
				pa = (pt->base_address() | (va & 0xfff));
				return true;
			}
			
			static inline bool quick_write(hva_t va, hpa_t& pa, bool user_mode)
			{
				table_idx_t pm_idx = 0, pdp_idx, pd_idx, pt_idx;
				va_table_indicies(va, pm_idx, pdp_idx, pd_idx, pt_idx);

				page_map_entry_t* pm;
				pm = &((page_map_t *)vm_phys_to_virt(CR3))->entries[pm_idx];
				if (!pm->present()) {
					return false;
				}

				page_dir_ptr_entry_t* pdp;
				pdp = &((page_dir_ptr_t *)vm_phys_to_virt(pm->base_address()))->entries[pdp_idx];
				if (!pdp->present()) {
					return false;
				}

				page_dir_entry_t* pd;
				pd = &((page_dir_t *)vm_phys_to_virt(pdp->base_address()))->entries[pd_idx];
				if (!pd->present()) {
					return false;
				}

				page_table_entry_t* pt = &((page_table_t *)vm_phys_to_virt(pd->base_address()))->entries[pt_idx];
				if (!pt->present() || !pt->writable() || (!pt->allow_user() && user_mode)) {
					return false;
				}
								
				pa = (pt->base_address() | (va & 0xfff));
				return true;
			}*/
		};
	}
}

#endif	/* MM_H */

