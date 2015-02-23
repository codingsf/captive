/*
 * File:   pl061.h
 * Author: spink
 *
 * Created on 23 February 2015, 10:40
 */

#ifndef PL061_H
#define	PL061_H

#include <devices/arm/primecell.h>

namespace captive {
	namespace devices {
		namespace irq {
			class IRQLine;
		}

		namespace arm {
			class PL061 : public Primecell
			{
			public:
				PL061(irq::IRQLine& irq);
				virtual ~PL061();

				virtual bool read(uint64_t off, uint8_t len, uint64_t& data);
				virtual bool write(uint64_t off, uint8_t len, uint64_t data);

			private:
				irq::IRQLine& _irq;
			};
		}
	}
}

#endif	/* PL061_H */

