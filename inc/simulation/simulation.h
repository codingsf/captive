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
			struct EventPacket {
				enum EventPacketType {
					MEMORY_READ = 0,
					MEMORY_WRITE = 1,
					INSTRUCTION_FETCH = 2,
				};
				
				uint8_t size : 4;
				EventPacketType type : 2;
				uint32_t unused: 6;
				uint32_t phys_page: 20;
				uint32_t address;
			} packed;
		
			void register_core(hypervisor::CPU& core) { _cores.push_back(&core); }
					
			virtual void process_events(const EventPacket *events, uint32_t count);
			
			virtual bool init() = 0;
			virtual void start() = 0;
			virtual void stop() = 0;
			
			virtual Events::Events required_events() const = 0;
			
			virtual void dump() = 0;
			
			virtual void begin_record();
			virtual void end_record();
			
		protected:
			const std::vector<hypervisor::CPU *>& cores() const { return _cores; }
			
		private:
			std::vector<hypervisor::CPU *> _cores;
		};
	}
}

#endif /* SIMULATION_H */

