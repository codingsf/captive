/*
 * File:   pl031.h
 * Author: spink
 *
 * Created on 23 February 2015, 12:02
 */

#ifndef PL031_H
#define	PL031_H

#include <devices/arm/primecell.h>
#include <devices/timers/timer-manager.h>
#include <devices/irq/irq-line.h>

#include <mutex>

namespace captive {
	namespace devices {
		namespace arm {
			class PL031 : public Primecell, public timers::TimerSink
			{
			public:
				PL031(timers::TimerManager& timer_manager, irq::IRQLine& irq);
				virtual ~PL031();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;
				
				std::string name() const { return "pl031"; }
				
				void timer_expired(uint64_t ticks) override;
				
			private:
				std::mutex lock;
				
				irq::IRQLine& irq;
				uint32_t dr;
				uint32_t match, load, ctrl, mask, isr;
				
				void update_irq();
			};
		}
	}
}

#endif	/* PL031_H */

