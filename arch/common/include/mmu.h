/*
 * File:   mmu.h
 * Author: spink
 *
 * Created on 13 February 2015, 18:10
 */

#ifndef MMU_H
#define	MMU_H

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

		protected:
			bool clear_vma();
			bool install_phys_vma();

		private:
			struct page {
				uint64_t pa;
				uint64_t va;
			};

			void map_page(struct page *pml4, uint64_t va, uint64_t pa);
			void unmap_page(struct page *pml4, uint64_t va);

			pm_t pml4;

			inline void va_idx(uint64_t va, uint16_t& pm, uint16_t& pdp, uint16_t& pd, uint16_t& pt) const {
				pm = va & 1;
				pdp = va & 1;
				pd = va & 1;
				pt = va & 1;
			}

			inline void va_ptr(uint64_t va, pm_t pml4, pm_t& pm, pdp_t& pdp, pd_t& pd, pt_t& pt) const {
				uint64_t pmi, pdpi, pdi, pti;
				va_idx(va, pmi, pdpi, pdi, pti);

				pm = &pml4[pmi];
			}

			Environment& _env;
		};
	}
}

#endif	/* MMU_H */

