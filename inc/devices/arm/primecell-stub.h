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
				PrimecellStub(uint32_t device_id, std::string name="stub", uint32_t size=0x1000);
				virtual ~PrimecellStub();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				virtual uint32_t size() const { return _size; }
				virtual std::string name() const { return _name; }

			private:
				std::string _name;
				uint32_t _size;
			};
		}
	}
}

#endif	/* PRIMECELL_STUB_H */

