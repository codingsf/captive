/*
 * File:   versatile-sic.h
 * Author: spink
 *
 * Created on 18 February 2015, 15:15
 */

#ifndef VERSATILE_SIC_H
#define	VERSATILE_SIC_H

#include <devices/device.h>
#include <devices/irq/irq-controller.h>
#include <atomic>

namespace captive {
	namespace devices {
		namespace arm {
			class VersatileSIC : public Device, public irq::IRQController<32u>
			{
			public:
				VersatileSIC(irq::IRQLine& irq);
				virtual ~VersatileSIC();

				virtual bool read(uint64_t off, uint8_t len, uint64_t& data);
				virtual bool write(uint64_t off, uint8_t len, uint64_t data);

				virtual uint32_t size() const { return 0x1000; }

				virtual std::string name() const { return "sic"; }

				void irq_raised(irq::IRQLine& line) override;
				void irq_rescinded(irq::IRQLine& line) override;

			private:
				irq::IRQLine& _irq;

				std::atomic<uint32_t> status;
				uint32_t enable_mask;
			};
		}
	}
}

#endif	/* VERSATILE_SIC_H */

