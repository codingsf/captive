/*
 * File:   pl080.h
 * Author: spink
 *
 * Created on 21 February 2015, 10:08
 */

#ifndef PL080_H
#define	PL080_H

#include <devices/arm/primecell.h>

namespace captive {
	namespace devices {
		namespace arm {
			class PL080 : public Primecell
			{
			public:
				PL080();
				virtual ~PL080();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				virtual std::string name() const { return "pl080"; }
			};
		}
	}
}

#endif	/* PL080_H */

