/*
 * File:   arm-mmu.h
 * Author: spink
 *
 * Created on 13 February 2015, 18:13
 */

#ifndef ARM_MMU_H
#define	ARM_MMU_H

#include <define.h>
#include <mmu.h>
#include <devices/coco.h>

namespace captive {
	namespace arch {
		namespace arm {
			namespace devices {
				class CoCo;
			}
		
			class arm_cpu;
			
			class arm_mmu : public MMU
			{
			public:
				arm_mmu(arm_cpu& cpu);
				virtual ~arm_mmu();

				bool enable() override;
				bool disable() override;

				bool enabled() const override {
					return _enabled;
				}
				
			protected:
				devices::CoCo& _coco;
				bool _enabled;
				
				inline arm_cpu& armcpu() const { return (arm_cpu&)cpu(); }
				
				inline void *resolve_guest_phys(gpa_t gpa) const __attribute__((pure)) {
					return (void *)(0x100000000ULL + (uint64_t)gpa);
				}
			};
			
			class arm_mmu_v5 : public arm_mmu
			{
			public:
				arm_mmu_v5(arm_cpu& cpu);
				virtual ~arm_mmu_v5();

			protected:
				bool resolve_gpa(gva_t va, gpa_t& pa, const access_info& info, resolution_fault& fault, bool have_side_effects) override;

			private:
				struct l1_descriptor {
					uint32_t data;

					enum l1_descriptor_type {
						TT_ENTRY_FAULT = 0,
						TT_ENTRY_COARSE = 1,
						TT_ENTRY_SECTION = 2,
						TT_ENTRY_FINE = 3,
					};

					inline l1_descriptor_type type() const {
						return (l1_descriptor_type)(data & 0x3);
					}

					inline uint8_t domain() const {
						return (data >> 5) & 0xf;
					}

					inline uint8_t ap() const {
						return (data >> 10) & 0x3;
					}

					inline uint8_t C() const {
						return (data >> 3) & 1;
					}

					inline uint8_t B() const {
						return (data >> 2) & 1;
					}

					inline uint32_t base_addr() const {
						switch (type()) {
						case TT_ENTRY_FAULT:
							return 0;
						case TT_ENTRY_COARSE:
							return data & ~0x3ffUL;
						case TT_ENTRY_SECTION:
							return data & ~0xfffffUL;
						case TT_ENTRY_FINE:
							return data & ~0xfffUL;
						default:
							return 0;
						}
					}
				} packed;

				struct l2_descriptor {
					enum l2_descriptor_type {
						TE_FAULT = 0,
						TE_LARGE = 1,
						TE_SMALL = 2,
						TE_TINY = 3,
					};

					uint32_t data;

					inline l2_descriptor_type type() const { return (l2_descriptor_type)(data & 0x3); }

					inline uint32_t base_addr() const {
						switch(type()) {
						case TE_FAULT:
							return 0;
						case TE_LARGE:
							return data & ~0xffffUL;
						case TE_SMALL:
							return data & ~0xfffUL;
						case TE_TINY:
							return data & ~0x3ffUL;
						default:
							return 0;
						}
					}

					inline uint8_t ap0() const {
						return (data & 0x30) >> 4;
					}
				} packed;

				static_assert(sizeof(l1_descriptor) == 4, "L1 Descriptor Structure must be 32-bits");
				static_assert(sizeof(l2_descriptor) == 4, "L2 Descriptor Structure must be 32-bits");

				enum arm_resolution_fault {
					NONE,
					OTHER,

					SECTION_FAULT,
					SECTION_DOMAIN,
					SECTION_PERMISSION,

					COARSE_FAULT,
					COARSE_DOMAIN,
					COARSE_PERMISSION,
				};

				bool check_access_perms(uint32_t ap, bool kernel_mode, bool is_write);
				bool resolve_coarse_page(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, l1_descriptor *l1);
				bool resolve_fine_page(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, l1_descriptor *l1);
				bool resolve_section(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, l1_descriptor *l1);
			};
			
			class arm_mmu_v6 : public arm_mmu
			{
			public:
				arm_mmu_v6(arm_cpu& cpu);
				virtual ~arm_mmu_v6();

			protected:
				bool resolve_gpa(gva_t va, gpa_t& pa, const access_info& info, resolution_fault& fault, bool have_side_effects) override;

			private:
				struct l1_descriptor {
					enum l1_descriptor_type {
						FAULT,
						COARSE_PAGE_TABLE,
						SECTION,
						SUPERSECTION,
					};
					
					union {
						uint32_t data;
						
						struct {
							uint32_t TYPE:2;
							uint32_t DATA:30;
						} anon packed;
						
						struct {
							uint32_t TYPE:2;
							uint32_t IGNORED:30;
						} fault packed;
						
						struct {
							uint32_t TYPE:2;
							uint32_t SBZ0:1;
							uint32_t NS:1;
							uint32_t SBZ1:1;
							uint32_t DOM:4;
							uint32_t P:1;
							uint32_t BASE:22;
							
							inline uint32_t base_address() const { return BASE << 10; }
						} coarse_page_table packed;
						
						struct {
							uint32_t TYPE:2;
							uint32_t B:1;
							uint32_t C:1;
							uint32_t XN:1;
							uint32_t DOM:4;
							uint32_t P:1;
							uint32_t AP:2;
							uint32_t TEX:3;
							uint32_t APX:1;
							uint32_t S:1;
							uint32_t NG:1;
							uint32_t SUPER_SECTION:1;
							uint32_t NS:1;
							uint32_t BASE:12;
							
							inline uint32_t base_address() const { return BASE << 20; }
						} section packed;
						
						struct {
							uint32_t TYPE:2;
							uint32_t B:1;
							uint32_t C:1;
							uint32_t XN:1;
							uint32_t IGN:4;
							uint32_t P:1;
							uint32_t AP:2;
							uint32_t TEX:3;
							uint32_t APX:1;
							uint32_t S:1;
							uint32_t NG:1;
							uint32_t SUPER_SECTION:1;
							uint32_t NS:1;
							uint32_t BASE:12;
						} supersection packed;
					};
					
					inline l1_descriptor_type type() const
					{
						if (anon.TYPE == 0 || anon.TYPE == 3) return FAULT;
						if (anon.TYPE == 1) return COARSE_PAGE_TABLE;
						
						if (anon.TYPE == 2)
						{
							if (section.SUPER_SECTION) return SUPERSECTION;
							else return SECTION;
						}
						
						return FAULT;
					}
				} packed;
				
				struct l2_descriptor {
					enum l2_descriptor_type {
						FAULT,
						LARGE_PAGE,
						SMALL_PAGE,
					};
					
					union {
						uint32_t data;
						
						struct {
							uint32_t TYPE0:1;
							uint32_t TYPE1:1;
							uint32_t DATA:30;
						} anon packed;
						
						struct {
							uint32_t TYPE:2;
							uint32_t IGNORED:30;
						} fault packed;
						
						struct {
							uint32_t TYPE:2;
							uint32_t B:1;
							uint32_t C:1;
							uint32_t AP:2;
							uint32_t SBZ:3;
							uint32_t APX:1;
							uint32_t S:1;
							uint32_t NG:1;
							uint32_t TEX:3;
							uint32_t XN:1;
							uint32_t BASE:16;
						} large packed;
						
						struct {
							uint32_t XN:1;
							uint32_t SMALL:1;
							uint32_t B:1;
							uint32_t C:1;
							uint32_t AP:2;
							uint32_t TEX:3;
							uint32_t APX:1;
							uint32_t S:1;
							uint32_t NG:1;
							uint32_t BASE:20;
							
							inline uint32_t base_address() const { return BASE << 12; }
						} small packed;
					};
					
					inline l2_descriptor_type type() const
					{
						if (anon.TYPE0 == 0 && anon.TYPE1 == 0) {
							return FAULT;
						} else if (anon.TYPE0 == 1 && anon.TYPE1 == 0) {
							return LARGE_PAGE;
						} else if (anon.TYPE1 == 1) {
							return SMALL_PAGE;
						}
						
						return FAULT;
					}
				} packed;
				
				enum arm_resolution_fault {
					NONE,
					OTHER,

					SECTION_FAULT,
					SECTION_DOMAIN,
					SECTION_PERMISSION,

					COARSE_FAULT,
					COARSE_DOMAIN,
					COARSE_PERMISSION,
				};

				bool resolve_section(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, const l1_descriptor *l1);
				bool resolve_coarse(gva_t va, gpa_t& pa, const access_info& info, arm_resolution_fault& fault, const l1_descriptor *l1);
			};
		}
	}
}

#endif	/* ARM_MMU_H */
