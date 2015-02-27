/*
 * File:   pl011.h
 * Author: spink
 *
 * Created on 12 February 2015, 19:57
 */

#ifndef PL011_H
#define	PL011_H

#include <devices/arm/primecell.h>
#include <deque>
#include <atomic>

#define IRQ_TXINTR (1 << 5)
#define IRQ_RXINTR (1 << 4)

namespace captive {
	namespace devices {
		namespace irq {
			class IRQLine;
		}

		namespace arm {
			class PL011 : public Primecell
			{
			public:
				PL011(irq::IRQLine& irq);
				virtual ~PL011();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				virtual std::string name() const { return "pl011"; }

			private:
				irq::IRQLine& _irq;

				uint32_t control_word;
				uint32_t baud_rate, fractional_baud, line_control;
				uint32_t irq_mask;

				std::atomic<uint32_t> irq_status;

				uint32_t flag_register;

				std::deque<char> fifo;

				void update_irq();

				inline void raise_tx_irq() {
					irq_status |= IRQ_TXINTR;
					update_irq();
				}

				inline void raise_rx_irq() {
					irq_status |= IRQ_RXINTR;
					update_irq();
				}
			};
		}
	}
}

#endif	/* PL011_H */

