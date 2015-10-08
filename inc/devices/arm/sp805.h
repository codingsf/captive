/* 
 * File:   pl081.h
 * Author: s0457958
 *
 * Created on 01 October 2015, 12:15
 */

#ifndef SP805_H
#define SP805_H


#include <devices/arm/primecell.h>

namespace captive {
	namespace devices {
		namespace arm {
			class SP805 : public Primecell
			{
			public:
				SP805();
				virtual ~SP805();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				virtual std::string name() const { return "sp805"; }
			};
		}
	}
}

#endif /* PL081_H */

