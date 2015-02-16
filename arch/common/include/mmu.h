/*
 * File:   mmu.h
 * Author: spink
 *
 * Created on 13 February 2015, 18:10
 */

#ifndef MMU_H
#define	MMU_H

#include <define.h>

namespace captive {
	namespace arch {
		class Environment;

		typedef uint64_t pte_t;
		typedef pte_t *pm_t;
		typedef pte_t *pdp_t;
		typedef pte_t *pd_t;
		typedef pte_t *pt_t;

		class MMU
		{
		public:
			MMU(Environment& env);
			virtual ~MMU();

			virtual bool enable() = 0;
			virtual bool disable() = 0;
			virtual bool enabled() const = 0;

			inline Environment& env() const { return _env; }

			virtual bool handle_fault(uint64_t va) = 0;

		private:
			uint64_t pml4_phys;
			pm_t pml4;

		protected:
			bool clear_vma();
			bool install_phys_vma();

			struct page {
				uint64_t pa;
				uint64_t va;
			};

			void map_page(struct page *pml4, uint64_t va, uint64_t pa);
			void unmap_page(struct page *pml4, uint64_t va);

			static unsigned long __force_order;

			inline unsigned long read_cr3() const {
				unsigned long val;
				asm volatile("mov %%cr3, %0\n\t" : "=r"(val), "=m"(__force_order));
				return val;
			}

			inline void write_cr3(unsigned long val) {
				asm volatile("mov %0, %%cr3" :: "r"(val), "m"(__force_order));
			}

			inline void flush_tlb() {
				write_cr3(read_cr3());
			}

			#define BITS(val, start, end) ((val >> start) & (((1 << (end - start + 1)) - 1)))

			inline void *phys_to_virt(uint64_t pa) const {
				if (pa >= 0 && pa < 0x10000000)
					return (void *)(0x200000000 + pa);
				else
					return NULL;
			}

			inline void va_idx(uint64_t va, uint16_t& pm, uint16_t& pdp, uint16_t& pd, uint16_t& pt) const {
				pm = BITS(va, 39, 47);
				pdp = BITS(va, 30, 38);
				pd = BITS(va, 21, 29);
				pt = BITS(va, 12, 20);
			}

			inline void va_entries(uint64_t va, pm_t& pm, pdp_t& pdp, pd_t& pd, pt_t& pt) const {
				uint16_t pm_idx, pdp_idx, pd_idx, pt_idx;
				va_idx(va, pm_idx, pdp_idx, pd_idx, pt_idx);

				//pm = &pml4[pm_idx];
				pm = &((pm_t)phys_to_virt(pml4_phys))[pm_idx];

				uint64_t pdp_phys = *pm & ~0xfffULL;
				pdp = &((pdp_t)phys_to_virt(pdp_phys))[pdp_idx];

				uint64_t pd_phys = *pdp & ~0xfffULL;
				pd = &((pd_t)phys_to_virt(pd_phys))[pd_idx];

				uint64_t pt_phys = *pd & ~0xfffULL;
				pt = &((pt_t)phys_to_virt(pt_phys))[pt_idx];
			}

			Environment& _env;
		};
	}
}

#endif	/* MMU_H */

