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
				bool resolve_gpa(gva_t va, gpa_t& pa) const override;

			private:
				ArmCPU& _cpu;
				devices::CoCo& _coco;
				bool _enabled;

				struct tt_entry {
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
							return data & 0x1ff;
						case TT_ENTRY_SECTION:
							return data & 0x7ffff;
						case TT_ENTRY_FINE:
							return data & 0x7ff;
						default:
							return 0;
						}
					}
				} packed;

				struct section_table_entry {
					uint32_t data;
				} packed;

				struct table_entry {
					uint32_t data;
				} packed;

				struct coarse_table_entry : table_entry {
				};

				struct fine_table_entry : table_entry {
				};

				static_assert(sizeof(tt_entry) == 4, "TT Entry Structure must be 32-bits");
				static_assert(sizeof(section_table_entry) == 4, "Section Table Entry Structure must be 32-bits");
				static_assert(sizeof(coarse_table_entry) == 4, "Coarse Table Entry Structure must be 32-bits");
				static_assert(sizeof(fine_table_entry) == 4, "Fine Table Entry Structure must be 32-bits");

				typedef tt_entry *tt_base;

				inline tt_base get_ttbr() const {
					tt_base virt_base = (tt_base)0x210000000;

					pm_t pm;
					pdp_t pdp;
					pd_t pd;
					pt_t pt;
					va_entries((uint64_t)virt_base, pm, pdp, pd, pt);

					*pt++ = 0x100000000 | _coco.TTBR0() | 1;
					*pt++ = 0x100000000 | (_coco.TTBR0() + 0x1000) | 1;
					*pt++ = 0x100000000 | (_coco.TTBR0() + 0x2000) | 1;
					*pt++ = 0x100000000 | (_coco.TTBR0() + 0x3000) | 1;

					return virt_base;
				}
			};
		}
	}
}

#endif	/* ARM_MMU_H */

