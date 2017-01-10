/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   juno.h
 * Author: s0457958
 *
 * Created on 12 September 2016, 13:08
 */

#ifndef JUNO_H
#define JUNO_H

#include <platform/platform.h>
#include <hypervisor/config.h>
#include <string>
#include <list>

namespace captive {	
	namespace platform {
		class Juno : public Platform
		{
		public:
			enum Variant
			{
				CORTEX_A72,
			};
			
			Juno(const util::config::Configuration& cfg, devices::timers::TimerManager& timer_manager, Variant variant);
			virtual ~Juno();
			
			const hypervisor::GuestConfiguration& config() const override { return cfg; }
			
			bool start() override;
			bool stop() override;
			
			void dump() const override;
			
		private:
			Variant variant;
			hypervisor::GuestConfiguration cfg;
		};
	}
}

#endif /* JUNO_H */

