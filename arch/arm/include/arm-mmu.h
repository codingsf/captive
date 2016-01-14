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
				
				static MMU *create(arm_cpu& cpu);
				
			protected:
				devices::CoCo& _coco;
				bool _enabled;
				
				inline arm_cpu& armcpu() const { return (arm_cpu&)cpu(); }
			};
			
			class arm_mmu_v6 : public arm_mmu
			{
			public:
				arm_mmu_v6(arm_cpu& cpu);
				virtual ~arm_mmu_v6();

			protected:
				bool resolve_gpa(const struct resolve_request& request, struct resolve_response& response, bool have_side_effects) override;

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
							inline uint32_t domain() const { return DOM; }
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
							inline uint32_t domain() const { return DOM; }
							inline uint32_t ap() const { return (AP | (APX << 2)); }
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
							
							inline uint32_t base_address() const { return BASE << 16; }
							inline uint32_t ap() const { return APX << 2 | AP; }
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
							inline uint32_t ap() const { return APX << 2 | AP; }
						} small packed;
					};
					
					inline l2_descriptor_type type() const
					{
						if (anon.TYPE1 == 0 && anon.TYPE0 == 0) {
							return FAULT;
						} else if (anon.TYPE1 == 0 && anon.TYPE0 == 1) {
							return LARGE_PAGE;
						} else if (anon.TYPE1 == 1) {
							return SMALL_PAGE;
						}
						
						return FAULT;
					}
				} packed;
				
				enum arm_resolution_fault {
					NONE				= 0,
					ALIGNMENT			= 1,
					INSTRUCTION_DEBUG	= 2,
					SECTION_ACCESS_BIT	= 3,
					INSTRUCTION_CACHE_MAINTENANCE = 4,
					SECTION_TRANSLATION	= 5,
					PAGE_ACCESS_BIT		= 6,
					PAGE_TRANSLATION	= 7,
					PRECISE_EXTERNAL_ABORT = 8,
					SECTION_DOMAIN		= 9,
					RESERVED			= 10,
					PAGE_DOMAIN			= 11,
					EXTERNAL_ABORT_L1	= 12,
					SECTION_PERMISSION	= 13,
					EXTERNAL_ABORT_L2	= 14,
					PAGE_PERMISSION		= 15,
				};

				bool check_access_perms(uint32_t ap, const struct resolve_request& request);
				bool resolve_section(const struct resolve_request& request, struct resolve_response& response, arm_resolution_fault& fault, const l1_descriptor *l1);
				bool resolve_coarse(const struct resolve_request& request, struct resolve_response& response, arm_resolution_fault& fault, const l1_descriptor *l1);
			};
		}
	}
}

#endif	/* ARM_MMU_H */
