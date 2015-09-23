/* 
 * File:   pl390.h
 * Author: s0457958
 *
 * Created on 22 September 2015, 14:04
 */

#ifndef PL390_H
#define PL390_H

#include <devices/arm/primecell.h>
#include <devices/irq/irq-controller.h>
#include <atomic>

namespace captive {
	namespace devices {
		namespace arm {
			class PL390 : public Primecell, public irq::IRQController<32u>
			{
			public:
				PL390(irq::IRQLine& irq, irq::IRQLine& fiq);
				virtual ~PL390();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				virtual std::string name() const { return "pl390"; }

			protected:
				void irq_raised(irq::IRQLine& line) override;
				void irq_rescinded(irq::IRQLine& line) override;

			private:
				irq::IRQLine& irq;
				irq::IRQLine& fiq;
			};
		}
	}
}

#endif /* PL390_H */
