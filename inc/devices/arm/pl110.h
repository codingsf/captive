/*
 * File:   pl110.h
 * Author: spink
 *
 * Created on 21 February 2015, 10:25
 */

#ifndef PL110_H
#define	PL110_H

#include <devices/arm/primecell.h>

namespace captive {
	namespace devices {
		namespace gfx {
			class VirtualScreen;
		}

		namespace arm {
			class PL110 : public Primecell
			{
			public:
				PL110(gfx::VirtualScreen& screen);
				virtual ~PL110();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				inline gfx::VirtualScreen& screen() const { return _screen; }
				
			private:
				gfx::VirtualScreen& _screen;
			};
		}
	}
}

#endif	/* PL110_H */

