/* 
 * File:   mptimer.h
 * Author: s0457958
 *
 * Created on 15 October 2015, 16:44
 */

#ifndef MPTIMER_H
#define MPTIMER_H

#include <devices/device.h>
#include <devices/timers/tick-source.h>

namespace captive {
	namespace devices {
		namespace irq {
			class IRQLine;
		}
		
		namespace arm {
			class MPTimer : public Device, public timers::TickSink
			{
			public:
				MPTimer(timers::TickSource& ts, irq::IRQLine& irq);
				virtual ~MPTimer();
				
				std::string name() const override { return "mptimer"; }
				
				uint32_t size() const override { return 0x100; }

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;
				
			protected:
				void tick(uint32_t period) override;

			private:
				timers::TickSource& ts;
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

