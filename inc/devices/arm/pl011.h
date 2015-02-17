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

#define IRQ_TXINTR (1 << 5)
#define IRQ_RXINTR (1 << 4)

namespace captive {
	namespace devices {
		namespace arm {
			class PL011 : public Primecell
			{
			public:
				PL011();
				virtual ~PL011();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

			private:
				uint32_t control_word;
				uint32_t baud_rate, fractional_baud, line_control;
				uint32_t irq_mask, irq_status;

				uint32_t flag_register;

				std::deque<char> fifo;
			};
		}
	}
}

#endif	/* PL011_H */

