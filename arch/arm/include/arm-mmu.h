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

			class ArmCPU;

			class ArmMMU : public MMU
			{
			public:
				ArmMMU(ArmCPU& cpu);
				~ArmMMU();

				bool enable() override;
				bool disable() override;

				bool enabled() const override {
					return _enabled;
				}

			protected:
				bool resolve_gpa(gva_t va, gpa_t& pa, const access_info& info, resolution_fault& fault, bool have_side_effects) override;

			private:
				devices::CoCo& _coco;
				bool _enabled;

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

				inline void *resolve_guest_phys(gpa_t gpa) {
					return (void *)(0x100000000ULL + (uint64_t)gpa);
				}
			};
		}
	}
}

#endif	/* ARM_MMU_H */

