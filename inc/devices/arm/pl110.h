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

		namespace irq {
			class IRQLine;
		}

		namespace arm {
			class PL110 : public Primecell
			{
			public:
				PL110(gfx::VirtualScreen& screen, irq::IRQLine& irq);
				virtual ~PL110();

				bool read(uint64_t off, uint8_t len, uint64_t& data) override;
				bool write(uint64_t off, uint8_t len, uint64_t data) override;

				inline gfx::VirtualScreen& screen() const { return _screen; }

				virtual std::string name() const { return "pl110"; }

			private:
				void update_control();
				void update_irq();

				gfx::VirtualScreen& _screen;
				irq::IRQLine& _irq;

				union {
					uint32_t data;
					struct {
						uint8_t en : 1;
						uint8_t bpp : 3;
						uint8_t bw : 1;
						uint8_t tft : 1;
						uint8_t mono8 : 1;
						uint8_t dual : 1;
						uint8_t bgr : 1;
						uint8_t bebo : 1;
						uint8_t bepo : 1;
						uint8_t lcd_pwr : 1;
						uint8_t lcd_vcomp : 2;
						uint8_t rsvd1 : 2;
						uint8_t watermark : 1;
						uint32_t rsvd0 : 15;
					} __packed fields;
				} control;

				uint32_t lcd_timing[4];

				uint32_t upper_fbbase, lower_fbbase;
				uint32_t isr, irq_mask;
			};
		}
	}
}

#endif	/* PL110_H */

