/* 
 * File:   fd-uart.h
 * Author: s0457958
 *
 * Created on 03 August 2015, 13:28
 */

#ifndef FD_UART_H
#define	FD_UART_H

#include <devices/io/uart.h>

namespace captive {
	namespace devices {
		namespace io {
			class FDUART : public UART
			{
			public:
				FDUART(int infd, int outfd);
				virtual ~FDUART();
				
				bool open() override;
				bool close() override;

				bool read_char(uint8_t& ch) override;
				bool write_char(uint8_t ch) override;
				
			private:
				int infd, outfd;
			};
		}
	}
}

#endif	/* FD_UART_H */

