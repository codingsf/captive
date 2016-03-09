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
#include <vector>

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
				GICCPUInterface(GIC& owner, irq::IRQLine& irq, int id);
				virtual ~GICCPUInterface();
				
				std::string name() const override { return "gic-cpu"; }
				uint32_t size() const override { return 0x100; }

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;
				
				bool enabled() const { return ctrl & 1; }
				
			private:
				GIC& owner;
				irq::IRQLine& irq;
				int id;
				
				uint32_t last_active[96];
				
				uint32_t ctrl, prio_mask, binpnt;				
				uint32_t current_pending, running_irq, running_priority;
				
				std::mutex update_lock;

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
					
				uint32_t cpu_targets[24];
				uint32_t config[6];
				
				inline bool enabled() const { return !!(ctrl & 1); }
				
				void set_enabled(uint32_t base, uint8_t bits);
				void clear_enabled(uint32_t base, uint8_t bits);
				void set_pending(uint32_t base, uint8_t bits);
				void clear_pending(uint32_t base, uint8_t bits);

				void sgi(uint32_t data);
			};

			class GIC : public irq::IRQController<96u>
			{
				friend class GICCPUInterface;
				friend class GICDistributorInterface;
				
			public:
				GIC();
				virtual ~GIC();
				
				void add_core(irq::IRQLine& irq, int id);
				
				GICCPUInterface& get_core(int id) { return *cores[id]; }
				GICDistributorInterface& get_distributor() { return distributor; }

			protected:
				void irq_raised(irq::IRQLine& line) override;
				void irq_rescinded(irq::IRQLine& line) override;

			private:
				std::mutex lock;
				
				std::vector<GICCPUInterface *> cores;
				GICDistributorInterface distributor;
				
				struct gic_irq {
					int index;
					bool enabled;
					bool pending;
					bool active;
					bool model;
					bool raised;
					bool edge_triggered;
					uint32_t priority;
				} irqs[96];
				
				gic_irq& get_gic_irq(int index)
				{
					assert(index < 96);
					return irqs[index];
				}
							
				void update();
			};
		}
	}
}

#endif /* GIC_H */