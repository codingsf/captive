/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   simulation.h
 * Author: s0457958
 *
 * Created on 01 September 2016, 16:14
 */

#ifndef SIMULATION_H
#define SIMULATION_H

#include <define.h>
#include <vector>

namespace captive
{
	namespace hypervisor
	{
		class CPU;
	}
	
	namespace simulation
	{
		namespace Events
		{
			enum Events
			{
				InstructionFetch = 1,
				MemoryRead = 2,
				MemoryWrite = 4,
				InstructionCount = 8,
			};
		}
		
		class Simulation
		{
		public:
			void register_core(hypervisor::CPU& core) { _cores.push_back(&core); }
			
			virtual void instruction_fetch(hypervisor::CPU& core, uint32_t virt_pc, uint32_t phys_pc);
			
			virtual bool init() = 0;
			virtual void start() = 0;
			virtual void stop() = 0;
			
			virtual Events::Events required_events() const = 0;
			
		protected:
			const std::vector<hypervisor::CPU *>& cores() const { return _cores; }
			
		private:
			std::vector<hypervisor::CPU *> _cores;
		};
	}
}

#endif /* SIMULATION_H */

