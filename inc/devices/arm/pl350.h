/* 
 * File:   pl081.h
 * Author: s0457958
 *
 * Created on 01 October 2015, 12:15
 */

#ifndef PL350_H
#define PL350_H


#include <devices/arm/primecell.h>

namespace captive {
	namespace devices {
		namespace arm {
			class PL350 : public Primecell
			{
			public:
				PL350();
				virtual ~PL350();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				virtual std::string name() const { return "pl131"; }
			};
		}
	}
}

#endif /* PL131_H */

