/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   cache-simulation.h
 * Author: s0457958
 *
 * Created on 01 September 2016, 16:13
 */

#ifndef CACHE_SIMULATION_H
#define CACHE_SIMULATION_H

#ifdef XXX
#include <define.h>
#include <stdio.h>

extern "C"
{
#include <d4.h>
}
#else
extern "C"
{
	struct d4cache;
}
#endif

#include <simulation/simulation.h>

namespace captive
{
	namespace simulation
	{
		namespace cache
		{
			class CacheSimulation : public Simulation
			{
			public:
				bool init() override;
				void start() override;
				void stop() override;
				
				void instruction_fetch(hypervisor::CPU& core, uint32_t virt_pc, uint32_t phys_pc) override;
				
				Events::Events required_events() const override { return Events::InstructionFetch; }
				
			private:
				d4cache *mm, *l2, *l1d, *l1i;
			};
		}
	}
}

#endif /* CACHE_SIMULATION_H */

