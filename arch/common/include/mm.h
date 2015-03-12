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

#define packed __attribute__((packed))

namespace captive {
	namespace arch {
		class MMU;

		namespace arm {
			class ArmMMU;
		}

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
				EXECUTED	= 1 << 9
			};

			uint64_t data;

			inline uint64_t base_address() const { return data & ~0xfffULL; }
			inline void base_address(uint64_t v) { data &= 0xfffULL; data |= v &~0xfffULL; }

			inline uint16_t flags() const { return data & 0xfffULL; }
			inline void flags(uint16_t v) { data &= ~0xfffULL; data |= v & 0xfffULL;}

			inline bool present() const { return (flags() & PRESENT) == PRESENT; }
			inline void present(bool v) { if (v) flags(flags() | PRESENT); else flags(flags() & ~PRESENT); }
			inline bool writable() const { return (flags() & WRITABLE) == WRITABLE; }
			inline void writable(bool v) { if (v) flags(flags() | WRITABLE); else flags(flags() & ~WRITABLE); }
			inline bool allow_user() const { return get_flag(ALLOW_USER); }
			inline void allow_user(bool v) { set_flag(ALLOW_USER, v); }

			inline bool executed() const { return get_flag(EXECUTED); }
			inline void executed(bool v) { set_flag(EXECUTED, v); }

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

		class Memory {
			friend class MMU;
			friend class arm::ArmMMU;

		public:
			struct Page {
				va_t va;
				pa_t pa;
			};

			Memory(uint64_t first_phys_page);

			static Page alloc_page();
			static void free_page(Page& page);
			static void map_page(va_t va, Page& page);

		private:
			static Memory *mm;

			pa_t _next_phys_page;
			va_t _data_base;

			static uint64_t __force_order;

			static inline pa_t read_cr3() {
				uint64_t val;
				asm volatile("mov %%cr3, %0\n\t" : "=r"(val), "=m"(__force_order));
				return (pa_t)val;
			}

			static inline void write_cr3(pa_t val) {
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

			static inline void flush_page(va_t addr) {
				asm volatile("invlpg (%0)\n" :: "r"((uint64_t)addr) : "memory");
			}

		public:
			static inline void flush_tlb() {
				write_cr3(read_cr3());
			}

			static inline void flush_tlb_all() {
				uint64_t cr4 = read_cr4();
				write_cr4(cr4 & ~(1 << 7));
				write_cr4(cr4);
			}

			#define BITS(val, start, end) ((((uint64_t)val) >> start) & (((1 << (end - start + 1)) - 1)))

			static inline va_t phys_to_virt(pa_t pa) {
				if ((uint64_t)pa >= 0 && (uint64_t)pa < 0x10000000) {
					return (va_t *)(0x200000000 + (uint64_t)pa);
				} else {
					return NULL;
				}
			}

			static inline void va_table_indicies(va_t va, table_idx_t& pm, table_idx_t& pdp, table_idx_t& pd, table_idx_t& pt) {
				pm = BITS(va, 39, 47);
				pdp = BITS(va, 30, 38);
				pd = BITS(va, 21, 29);
				pt = BITS(va, 12, 20);
			}

			static inline void get_va_table_entries(va_t va, page_map_entry_t*& pm, page_dir_ptr_entry_t*& pdp, page_dir_entry_t*& pd, page_table_entry_t*& pt, bool allocate=true) {
				table_idx_t pm_idx, pdp_idx, pd_idx, pt_idx;
				va_table_indicies(va, pm_idx, pdp_idx, pd_idx, pt_idx);

				// L4
				pm = &((page_map_t *)phys_to_virt(read_cr3()))->entries[pm_idx];
				if (pm->base_address() == 0) {
					auto page = Memory::alloc_page();
					pm->base_address((uint64_t)page.pa);
					pm->present(true);
					pm->writable(true);
					pm->allow_user(true);
				}

				// L3
				pdp = &((page_dir_ptr_t *)phys_to_virt((pa_t)(uint64_t)pm->base_address()))->entries[pdp_idx];
				if (pdp->base_address() == 0) {
					auto page = Memory::alloc_page();
					pdp->base_address((uint64_t)page.pa);
					pdp->present(true);
					pdp->writable(true);
					pdp->allow_user(true);
				}

				// L2
				pd = &((page_dir_t *)phys_to_virt((pa_t)(uint64_t)pdp->base_address()))->entries[pd_idx];
				if (pd->base_address() == 0) {
					auto page = Memory::alloc_page();
					pd->base_address((uint64_t)page.pa);
					pd->present(true);
					pd->writable(true);
					pd->allow_user(true);
				}

				pt = &((page_table_t *)phys_to_virt((pa_t)(uint64_t)pd->base_address()))->entries[pt_idx];
			}

			#undef BITS

			static inline void set_va_flags(va_t va, uint32_t flags)
			{
				page_map_entry_t* pm;
				page_dir_ptr_entry_t* pdp;
				page_dir_entry_t* pd;
				page_table_entry_t* pt;

				get_va_table_entries(va, pm, pdp, pd, pt);
				pt->flags(flags);

				flush_page(va);
			}
		};
	}
}

#endif	/* MM_H */

