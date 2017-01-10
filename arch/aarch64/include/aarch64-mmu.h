/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   aarch64-mmu.h
 * Author: s0457958
 *
 * Created on 12 September 2016, 12:55
 */

#ifndef AARCH64_MMU_H
#define AARCH64_MMU_H


#include <define.h>
#include <mmu.h>

namespace captive {
	namespace arch {
		namespace aarch64 {
			class aarch64_cpu;
			
			class aarch64_mmu : public MMU
			{
			public:
				aarch64_mmu(aarch64_cpu& cpu);
				virtual ~aarch64_mmu();

				bool enable() override;
				bool disable() override;

				bool enabled() const override {
					return _enabled;
				}
				
				bool resolve_gpa(resolution_context& context, bool have_side_effects) override;
				
				static MMU *create(aarch64_cpu& cpu);
				
			protected:
				bool _enabled;
			};
		}
	}
}

#endif /* AARCH64_MMU_H */

