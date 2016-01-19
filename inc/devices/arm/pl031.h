/*
 * File:   pl031.h
 * Author: spink
 *
 * Created on 23 February 2015, 12:02
 */

#ifndef PL031_H
#define	PL031_H

#include <devices/arm/primecell.h>

namespace captive {
	namespace devices {
		namespace arm {
			class PL031 : public Primecell
			{
			public:
				PL031();
				virtual ~PL031();

				virtual bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				virtual bool write(uint64_t off, uint8_t len, uint64_t data) override;
				
				virtual std::string name() const { return "pl031"; }
				
			private:
				uint32_t match, load, ctrl, mask, isr;
			};
		}
	}
}

#endif	/* PL031_H */

