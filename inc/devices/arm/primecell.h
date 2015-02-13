/*
 * File:   primecell.h
 * Author: spink
 *
 * Created on 12 February 2015, 19:57
 */

#ifndef PRIMECELL_H
#define	PRIMECELL_H

#include <devices/device.h>

namespace captive {
	namespace devices {
		namespace arm {
			class Primecell : public Device
			{
			public:
				Primecell(uint32_t peripheral_id);
				virtual ~Primecell();

				virtual bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				virtual bool write(uint64_t off, uint8_t len, uint64_t data) override;

			private:
				uint32_t peripheral_id;
				uint32_t primecell_id;
			};
		}
	}
}

#endif	/* PRIMECELL_H */

