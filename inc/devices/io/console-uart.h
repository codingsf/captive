/* 
 * File:   console-uart.h
 * Author: s0457958
 *
 * Created on 03 August 2015, 12:00
 */

#ifndef CONSOLE_UART_H
#define	CONSOLE_UART_H

#include <devices/io/fd-uart.h>
#include <termios.h>

namespace captive {
	namespace devices {
		namespace io {
			class ConsoleUART : public FDUART
			{
			public:
				ConsoleUART();
				virtual ~ConsoleUART();

				bool open() override;
				bool close() override;
				
			private:
				struct termios orig_settings;
			};
		}
	}
}

#endif	/* CONSOLE_UART_H */
