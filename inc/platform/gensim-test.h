/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   gensim-test.h
 * Author: s0457958
 *
 * Created on 09 March 2016, 12:36
 */

#ifndef GENSIM_TEST_H
#define GENSIM_TEST_H


#include <platform/platform.h>
#include <hypervisor/config.h>

namespace captive {
	namespace platform {
		class GensimTest : public Platform
		{
		public:
			GensimTest(const util::config::Configuration& cfg, devices::timers::TimerManager& timer_manager);
			virtual ~GensimTest();
			
			const hypervisor::GuestConfiguration& config() const override;
			
			bool start() override;
			bool stop() override;
			
		private:
			hypervisor::GuestConfiguration cfg;
		};
	}
}

#endif /* GENSIM_TEST_H */

