/*
 * File:   sp810.h
 * Author: spink
 *
 * Created on 17 February 2015, 16:00
 */

#ifndef SP810_H
#define	SP810_H

#include <devices/arm/primecell.h>
#include <chrono>

namespace captive {
	namespace devices {
		namespace arm {
			class SP810 : public Primecell
			{
			public:
				SP810();
				virtual ~SP810();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				virtual std::string name() const { return "sp810"; }

			private:
				typedef std::chrono::high_resolution_clock clock_t;
				typedef std::chrono::duration<uint32_t, std::ratio<1, 24000000> > tick_24MHz_t;
				typedef std::chrono::duration<uint32_t, std::ratio<1, 100> > tick_100Hz_t;

				clock_t::time_point hr_begin;

				uint32_t leds, lockval, colour_mode;
			};
		}
	}
}

#endif	/* SP810_H */

