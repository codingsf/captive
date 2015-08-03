/* 
 * File:   uart.h
 * Author: s0457958
 *
 * Created on 03 August 2015, 11:48
 */

#ifndef UART_H
#define	UART_H

#include <define.h>

namespace captive {
	namespace devices {
		namespace io {
			class UART {
			public:
				UART();
				virtual ~UART();
				
				virtual bool open() = 0;
				virtual bool close() = 0;
				
				virtual bool read_char(uint8_t& ch) = 0;
				virtual bool write_char(uint8_t ch) = 0;
			};
		}
	}
}

#endif	/* UART_H */

