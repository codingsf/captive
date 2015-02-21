/*
 * File:   primecell-stub.h
 * Author: spink
 *
 * Created on 21 February 2015, 10:21
 */

#ifndef PRIMECELL_STUB_H
#define	PRIMECELL_STUB_H

#include <devices/arm/primecell.h>

namespace captive {
	namespace devices {
		namespace arm {
			class PrimecellStub : public Primecell
			{
			public:
				PrimecellStub(uint32_t device_id);
				virtual ~PrimecellStub();
				
				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;
			};
		}
	}
}

#endif	/* PRIMECELL_STUB_H */

