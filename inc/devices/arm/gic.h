/* 
 * File:   gic.h
 * Author: s0457958
 *
 * Created on 29 September 2015, 10:38
 */

#ifndef GIC_H
#define GIC_H

#include <devices/device.h>
#include <devices/irq/irq-controller.h>

#include <set>
#include <mutex>

namespace captive {
	namespace devices {
		namespace arm {
			class GIC : public Device, public irq::IRQController<96u>
			{
			public:
				GIC(irq::IRQLine& irq);
				virtual ~GIC();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				std::string name() const override { return "gic"; }
				uint32_t size() const override { return 0x2000; }

			protected:
				void irq_raised(irq::IRQLine& line) override;
				void irq_rescinded(irq::IRQLine& line) override;

			private:
				std::mutex lock;
				
				irq::IRQLine& irq;
				std::set<uint32_t> asserted, pending;
				uint32_t current_pending, current_running;
				
				struct {
					uint32_t ctrl, prio_mask, binpnt;
				} cpu;
				
				struct {
					uint32_t ctrl, type;
					
					uint32_t set_enable[3], clear_enable[3], set_pending[3], clear_pending[3];
					
					uint32_t prio[24];
					uint32_t cpu_targets[24];
					uint32_t config[6];
				} distrib;
				
				uint32_t acknowledge();
				void complete(uint32_t irq);

				void update();
			};
		}
	}
}

#endif /* GIC_H */
