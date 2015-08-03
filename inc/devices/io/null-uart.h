/* 
 * File:   null-uart.h
 * Author: s0457958
 *
 * Created on 03 August 2015, 11:53
 */

#ifndef NULL_UART_H
#define	NULL_UART_H

#include <devices/io/uart.h>

namespace captive {
	namespace devices {
		namespace io {
			class NullUART : public UART
			{
			public:
				NullUART();
				virtual ~NullUART();

				bool open() override;
				bool close() override;

				bool read_char(uint8_t& ch) override;
				bool write_char(uint8_t ch) override;
			};
		}
	}
}

#endif	/* NULL_UART_H */

