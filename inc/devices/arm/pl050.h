/*
 * File:   pl050.h
 * Author: spink
 *
 * Created on 23 February 2015, 12:34
 */

#ifndef PL050_H
#define	PL050_H

#include <devices/arm/primecell.h>

namespace captive {
	namespace devices {
		namespace io {
			class PS2Device;
		}

		namespace arm {
			class PL050 : public Primecell
			{
			public:
				PL050(io::PS2Device& ps2);
				virtual ~PL050();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

			private:
				io::PS2Device& _ps2;
			};
		}
	}
}

#endif	/* PL050_H */

