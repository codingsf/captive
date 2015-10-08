/* 
 * File:   pl081.h
 * Author: s0457958
 *
 * Created on 01 October 2015, 12:15
 */

#ifndef PL180_H
#define PL180_H


#include <devices/arm/primecell.h>

namespace captive {
	namespace devices {
		namespace arm {
			class PL180 : public Primecell
			{
			public:
				PL180();
				virtual ~PL180();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				virtual std::string name() const { return "pl180"; }
			};
		}
	}
}

#endif /* PL081_H */

