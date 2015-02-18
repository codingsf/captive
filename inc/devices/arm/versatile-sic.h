/*
 * File:   versatile-sic.h
 * Author: spink
 *
 * Created on 18 February 2015, 15:15
 */

#ifndef VERSATILE_SIC_H
#define	VERSATILE_SIC_H

#include <devices/device.h>

namespace captive {
	namespace devices {
		namespace arm {
			class VersatileSIC : public Device
			{
			public:
				VersatileSIC();
				virtual ~VersatileSIC();

				virtual bool read(uint64_t off, uint8_t len, uint64_t& data);
				virtual bool write(uint64_t off, uint8_t len, uint64_t data);

			private:
				uint32_t status;
				uint32_t enable_mask;
			};
		}
	}
}

#endif	/* VERSATILE_SIC_H */

