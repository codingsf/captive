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

namespace captive {
	namespace devices {
		namespace arm {
			class GIC;
			class GICCPUInterface;
			class GICDistributorInterface;
			
			class GICCPUInterface : public Device
			{
				friend class GIC;
				friend class GICDistributorInterface;
				
			public:
				GICCPUInterface(GIC& owner, irq::IRQLine& irq);
				virtual ~GICCPUInterface();
				
				std::string name() const override { return "gic-cpu"; }
				uint32_t size() const override { return 0x100; }

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;
				
			private:
				GIC& owner;
				irq::IRQLine& irq;
				uint32_t ctrl, prio_mask, binpnt;
				
				std::set<uint32_t> asserted, pending;
				uint32_t current_pending, current_running;

				void update();
				uint32_t acknowledge();
				void complete(uint32_t irq);
			};
			
			class GICDistributorInterface : public Device
			{
				friend class GIC;
				friend class GICCPUInterface;
				
			public:
				GICDistributorInterface(GIC& owner);
				virtual ~GICDistributorInterface();
				
				std::string name() const override { return "gic-distributor"; }
				uint32_t size() const override { return 0x1000; }
				
				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;
				
			private:
				GIC& owner;
				uint32_t ctrl;
					
				uint32_t set_enable[3], clear_enable[3], set_pending[3], clear_pending[3];

				uint32_t prio[24];
				uint32_t cpu_targets[24];
				uint32_t config[6];
				
				inline bool enabled() const { return !!(ctrl & 1); }
				
				void post_interrupts();
			};

			class GIC : public irq::IRQController<96u>
			{
				friend class GICCPUInterface;
				friend class GICDistributorInterface;
				
			public:
				GIC(irq::IRQLine& irq0, irq::IRQLine& irq1);
				virtual ~GIC();
				
				GICCPUInterface& get_cpu(int id) { return cpu[id]; }
				GICDistributorInterface& get_distributor() { return distributor; }

			protected:
				void irq_raised(irq::IRQLine& line) override;
				void irq_rescinded(irq::IRQLine& line) override;

			private:
				GICCPUInterface cpu[2];
				GICDistributorInterface distributor;
				
				std::set<uint32_t> raised;
												
				void update();
			};
		}
	}
}

#endif /* GIC_H */
