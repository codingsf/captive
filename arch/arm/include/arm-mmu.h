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
				bool resolve_gpa(gva_t va, gpa_t& pa, resolution_fault& fault) override;

			private:
				ArmCPU& _cpu;
				devices::CoCo& _coco;
				bool _enabled;

				struct l0_entry {
					uint32_t data;

					enum tt_entry_type {
						TT_ENTRY_FAULT = 0,
						TT_ENTRY_COARSE = 1,
						TT_ENTRY_SECTION = 2,
						TT_ENTRY_FINE = 3,
					};

					inline tt_entry_type type() const {
						return (tt_entry_type)(data & 0x3);
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
							return data & ~0x3ffULL;
						case TT_ENTRY_SECTION:
							return data & ~0xfffffULL;
						case TT_ENTRY_FINE:
							return data & ~0xfffULL;
						default:
							return 0;
						}
					}
				} packed;

				struct l1_entry {
					enum table_entry_type {
						TE_FAULT = 0,
						TE_LARGE = 1,
						TE_SMALL = 2,
						TE_TINY = 3,
					};

					uint32_t data;

					inline table_entry_type type() const { return (table_entry_type)(data & 0x3); }

					inline uint32_t base_addr() const {
						switch(type()) {
						case TE_FAULT:
							return 0;
						case TE_LARGE:
							return data & ~0xffff;
						case TE_SMALL:
							return data & ~0xfff;
						case TE_TINY:
							return data & ~0x3ff;
						}
					}
				} packed;

				static_assert(sizeof(l0_entry) == 4, "TT Entry Structure must be 32-bits");
				static_assert(sizeof(l1_entry) == 4, "Table Entry Structure must be 32-bits");

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

				bool resolve_coarse_page(gva_t va, gpa_t& pa, arm_resolution_fault& fault, l0_entry *entry);
				bool resolve_fine_page(gva_t va, gpa_t& pa, arm_resolution_fault& fault, l0_entry *entry);
				bool resolve_section(gva_t va, gpa_t& pa, arm_resolution_fault& fault, l0_entry *entry);

				va_t temp_map(va_t base, gpa_t gpa, int n);
			};
		}
	}
}

#endif	/* ARM_MMU_H */

