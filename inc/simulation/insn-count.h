/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   insn-count.h
 * Author: s0457958
 *
 * Created on 01 September 2016, 16:38
 */

#ifndef INSN_COUNT_H
#define INSN_COUNT_H

#include <simulation/simulation.h>

namespace captive
{
	namespace simulation
	{
		class InstructionCounter : public Simulation
		{
		public:
			InstructionCounter() : _count(0) { }
			
			virtual void instruction_fetch(hypervisor::CPU& core, uint32_t virt_pc, uint32_t phys_pc, uint8_t size) override;
			
			Events::Events required_events() const override { return (Events::Events)(Events::InstructionFetch | Events::InstructionCount); }

			bool init() override;
			void start() override;
			void stop() override;
			void dump() override;
			
		private:
			uint64_t _count;
		};
	}
}

#endif /* INSN_COUNT_H */

