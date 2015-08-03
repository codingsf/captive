/* 
 * File:   console-uart.h
 * Author: s0457958
 *
 * Created on 03 August 2015, 12:00
 */

#ifndef CONSOLE_UART_H
#define	CONSOLE_UART_H

#include <devices/io/uart.h>

namespace captive {
	namespace devices {
		namespace io {
			class ConsoleUART : public UART
			{
			public:
				ConsoleUART();
				virtual ~ConsoleUART();

				virtual bool open();
				virtual bool close();

				virtual bool read_char(uint8_t& ch);
				virtual bool write_char(uint8_t ch);
			};
		}
	}
}

#endif	/* CONSOLE_UART_H */
