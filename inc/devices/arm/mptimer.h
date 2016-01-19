/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   mptimer.h
 * Author: s0457958
 *
 * Created on 08 January 2016, 16:47
 */

#ifndef MPTIMER_H
#define MPTIMER_H

#include <devices/device.h>
#include <devices/timers/timer-manager.h>

namespace captive {
	namespace devices {
		namespace irq {
			class IRQLine;
		}
		
		namespace arm {
			class MPTimer : public Device, public timers::TimerSink
			{
			public:
				MPTimer(timers::TimerManager& timer_manager, irq::IRQLine& irq);
				virtual ~MPTimer();
				
				std::string name() const override { return "mptimer"; }
				
				uint32_t size() const override { return 0x100; }

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;
				
				void timer_expired(uint64_t ticks) override;

			private:
				timers::TimerManager& timer_manager;
				irq::IRQLine& irq;
				
				bool enabled, auto_reload, irq_enabled;
				uint8_t prescale;
				uint32_t load, val, isr;
				uint64_t start;
				
				void update_irq();
			};
		}
	}
}

#endif /* MPTIMER_H */

