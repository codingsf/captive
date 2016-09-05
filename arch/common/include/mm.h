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

		struct entry_t {
			enum entry_flags_t {
				// PTE
				PRESENT		= 1 << 0,
				WRITABLE	= 1 << 1,
				ALLOW_USER	= 1 << 2,
				WRITE_THROUGH	= 1 << 3,
				CACHE_DISABLED	= 1 << 4,
				ACCESSED	= 1 << 5,
				DIRTY		= 1 << 6,
				RESERVED	= 1 << 7,
				GLOBAL		= 1 << 8,

				// CUSTOM
				EXECUTED	= 1 << 9,
				DEVICE		= 1 << 10,
				EXECUTABLE	= 1 << 11,
			};

			uint64_t data;

			inline hpa_t base_address() const { return data & ~0xfffULL; }
			inline void base_address(hpa_t v) { data &= 0xfffULL; data |= v &~0xfffULL; }

			inline uint16_t flags() const { return data & 0xfffULL; }
			inline void flags(uint16_t v) { data &= ~0xfffULL; data |= v & 0xfffULL;}

			inline bool present() const { return (flags() & PRESENT) == PRESENT; }
			inline void present(bool v) { if (v) flags(flags() | PRESENT); else flags(flags() & ~PRESENT); }

			inline bool dirty() const { return (flags() & DIRTY) == DIRTY; }
			inline void dirty(bool v) { if (v) flags(flags() | DIRTY); else flags(flags() & ~DIRTY); }

			inline bool writable() const { return (flags() & WRITABLE) == WRITABLE; }
			inline void writable(bool v) { if (v) flags(flags() | WRITABLE); else flags(flags() & ~WRITABLE); }

			inline bool allow_user() const { return get_flag(ALLOW_USER); }
			inline void allow_user(bool v) { set_flag(ALLOW_USER, v); }

			inline bool executed() const { return get_flag(EXECUTED); }
			inline void executed(bool v) { set_flag(EXECUTED, v); }

			inline bool executable() const { return get_flag(EXECUTABLE); }
			inline void executable(bool v) { set_flag(EXECUTABLE, v); }

			inline bool device() const { return get_flag(DEVICE); }
			inline void device(bool v) { set_flag(DEVICE, v); }

			inline bool cache_disabled() const { return get_flag(CACHE_DISABLED); }
			inline void cache_disabled(bool v) { set_flag(CACHE_DISABLED, v); }

			inline bool write_through() const { return get_flag(WRITE_THROUGH); }
			inline void write_through(bool v) { set_flag(WRITE_THROUGH, v); }

			inline bool get_flag(entry_flags_t flag) const {
				return (flags() & (uint16_t)flag) == (uint16_t)flag;
			}

			inline void set_flag(entry_flags_t flag, bool v) {
				if (v) {
					flags(flags() | (uint16_t)flag);
				} else {
					flags(flags() & ~(uint16_t)flag);
				}
			}

			inline void dump() const {
				printf("mm: entry: data=%016lx, addr=%lx, flags=%x\n", data, base_address(), flags());
			}
		} packed;

		static_assert(sizeof(entry_t) == 8, "x86 page table entry must be 64-bits");

		typedef entry_t page_map_entry_t;
		typedef entry_t page_dir_ptr_entry_t;
		typedef entry_t page_dir_entry_t;
		typedef entry_t page_table_entry_t;

		struct page_map_t {
			page_map_entry_t entries[4];
		} packed;

		static_assert(sizeof(page_map_t) == 32, "x86 page map must be 32 bytes");

		struct page_dir_ptr_t {
			page_dir_ptr_entry_t entries[512];
		} packed;

		static_assert(sizeof(page_dir_ptr_t) == 0x1000, "x86 page directory pointer table must be 4096 bytes");

		struct page_dir_t {
			page_dir_entry_t entries[512];
		} packed;

		static_assert(sizeof(page_dir_t) == 0x1000, "x86 page directory table must be 4096 bytes");

		struct page_table_t {
			page_table_entry_t entries[512];
		} packed;

		static_assert(sizeof(page_table_t) == 0x1000, "x86 page table must be 4096 bytes");

		typedef uint16_t table_idx_t;
		
#define CR3 (hpa_t)0x40091000

		class Memory {
			friend class MMU;

		public:
			static void init();
			
		private:
			Memory();
			
			static uint64_t __force_order;

			static inline hpa_t read_cr3() {
				uint64_t val;
				asm volatile("mov %%cr3, %0\n\t" : "=r"(val), "=m"(__force_order));
				return (hpa_t)val;
			}

			static inline void write_cr3(hpa_t val) {
				asm volatile("mov %0, %%cr3" :: "r"((uint64_t)val), "m"(__force_order));
			}

			static inline uint64_t read_cr4() {
				uint64_t val;
				asm volatile("mov %%cr4, %0\n\t" : "=r"(val), "=m"(__force_order));
				return val;
			}

			static inline void write_cr4(uint64_t val) {
				asm volatile("mov %0, %%cr4" :: "r"(val), "m"(__force_order));
			}
			
			static uintptr_t alloc_pgt();

		public:
			static void disable_caching(hva_t addr);
			
			static inline void flush_page(hva_t addr) {
				asm volatile("invlpg (%0)\n" :: "r"((uint64_t)addr) : "memory");
			}

			static inline void flush_tlb() {
				write_cr3(CR3);
			}

			static inline void flush_tlb_all() {
				uint64_t cr4 = read_cr4();
				write_cr4(cr4 & ~(1 << 7));
				write_cr4(cr4);
			}
			
			static inline void wbinvd() {
				asm volatile("wbinvd" ::: "memory");
			}

			#define BITS(val, start, end) ((((uint64_t)val) >> start) & (((1 << (end - start + 1)) - 1)))
			
			static inline void va_table_indicies(hva_t va, table_idx_t& pm, table_idx_t& pdp, table_idx_t& pd, table_idx_t& pt) __attribute__((pure)) {
				pm = BITS(va, 39, 47);
				pdp = BITS(va, 30, 38);
				pd = BITS(va, 21, 29);
				pt = BITS(va, 12, 20);
			}

			static inline void get_va_table_entries(hva_t va, page_map_entry_t*& pm, page_dir_ptr_entry_t*& pdp, page_dir_entry_t*& pd, page_table_entry_t*& pt, bool allocate=true) {
				table_idx_t pm_idx = 0, pdp_idx, pd_idx, pt_idx;
				va_table_indicies(va, pm_idx, pdp_idx, pd_idx, pt_idx);

				// L4 read_cr3()
				pm = &((page_map_t *)HPA_TO_HVA(CR3))->entries[pm_idx];
				if (pm->base_address() == 0) {
					pm->base_address(alloc_pgt());
					pm->present(true);
					pm->writable(true);
					pm->allow_user(true);
				}

				// L3
				pdp = &((page_dir_ptr_t *)HPA_TO_HVA(pm->base_address()))->entries[pdp_idx];
				if (pdp->base_address() == 0) {
					pdp->base_address(alloc_pgt());
					pdp->present(true);
					pdp->writable(true);
					pdp->allow_user(true);
				}

				// L2
				pd = &((page_dir_t *)HPA_TO_HVA(pdp->base_address()))->entries[pd_idx];
				if (pd->base_address() == 0) {
					pd->base_address(alloc_pgt());
					pd->present(true);
					pd->writable(true);
					pd->allow_user(true);
				}

				pt = &((page_table_t *)HPA_TO_HVA(pd->base_address()))->entries[pt_idx];
			}

			#undef BITS

			static inline uint32_t get_va_flags(hva_t va)
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
				pm = &((page_map_t *)HPA_TO_HVA(CR3))->entries[pm_idx];
				if (!pm->present()) {
					return false;
				}

				page_dir_ptr_entry_t* pdp;
				pdp = &((page_dir_ptr_t *)HPA_TO_HVA(pm->base_address()))->entries[pdp_idx];
				if (!pdp->present()) {
					return false;
				}

				page_dir_entry_t* pd;
				pd = &((page_dir_t *)HPA_TO_HVA(pdp->base_address()))->entries[pd_idx];
				if (!pd->present()) {
					return false;
				}

				page_table_entry_t* pt = &((page_table_t *)HPA_TO_HVA(pd->base_address()))->entries[pt_idx];
				if (!pt->present()) {
					return false;
				}
								
				pa = pt->base_address();
				return true;
			}
			
			static inline bool quick_fetch(hva_t va, hpa_t& pa, bool user_mode)
			{
				table_idx_t pm_idx = 0, pdp_idx, pd_idx, pt_idx;
				va_table_indicies(va, pm_idx, pdp_idx, pd_idx, pt_idx);

				page_map_entry_t* pm;
				pm = &((page_map_t *)HPA_TO_HVA(CR3))->entries[pm_idx];
				if (!pm->present()) {
					return false;
				}

				page_dir_ptr_entry_t* pdp;
				pdp = &((page_dir_ptr_t *)HPA_TO_HVA(pm->base_address()))->entries[pdp_idx];
				if (!pdp->present()) {
					return false;
				}

				page_dir_entry_t* pd;
				pd = &((page_dir_t *)HPA_TO_HVA(pdp->base_address()))->entries[pd_idx];
				if (!pd->present()) {
					return false;
				}

				page_table_entry_t* pt = &((page_table_t *)HPA_TO_HVA(pd->base_address()))->entries[pt_idx];
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
				pm = &((page_map_t *)HPA_TO_HVA(CR3))->entries[pm_idx];
				if (!pm->present()) {
					return false;
				}

				page_dir_ptr_entry_t* pdp;
				pdp = &((page_dir_ptr_t *)HPA_TO_HVA(pm->base_address()))->entries[pdp_idx];
				if (!pdp->present()) {
					return false;
				}

				page_dir_entry_t* pd;
				pd = &((page_dir_t *)HPA_TO_HVA(pdp->base_address()))->entries[pd_idx];
				if (!pd->present()) {
					return false;
				}

				page_table_entry_t* pt = &((page_table_t *)HPA_TO_HVA(pd->base_address()))->entries[pt_idx];
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
				pm = &((page_map_t *)HPA_TO_HVA(CR3))->entries[pm_idx];
				if (!pm->present()) {
					return false;
				}

				page_dir_ptr_entry_t* pdp;
				pdp = &((page_dir_ptr_t *)HPA_TO_HVA(pm->base_address()))->entries[pdp_idx];
				if (!pdp->present()) {
					return false;
				}

				page_dir_entry_t* pd;
				pd = &((page_dir_t *)HPA_TO_HVA(pdp->base_address()))->entries[pd_idx];
				if (!pd->present()) {
					return false;
				}

				page_table_entry_t* pt = &((page_table_t *)HPA_TO_HVA(pd->base_address()))->entries[pt_idx];
				if (!pt->present() || !pt->writable() || (!pt->allow_user() && user_mode)) {
					return false;
				}
								
				pa = (pt->base_address() | (va & 0xfff));
				return true;
			}
		};
	}
}

#endif	/* MM_H */

