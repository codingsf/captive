/* 
 * File:   pl310.h
 * Author: Tom Spink <t.spink@sms.ed.ac.uk>
 *
 * Created on 26 October 2015, 13:33
 */

#ifndef PL310_H
#define PL310_H

#include <devices/device.h>

namespace captive {
	namespace devices {
		namespace arm {
			class PL310 : public Device
			{
			public:
				PL310();
				virtual ~PL310();
				
				uint32_t size() const override { return 0x1000; }

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				std::string name() const override { return "pl310"; }
				
			private:
				uint32_t ctrl, aux, tag, dc, filt_start, filt_end;
			};
		}
	}
}

#endif /* PL310_H */

