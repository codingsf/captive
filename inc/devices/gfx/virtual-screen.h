/*
 * File:   virtual-screen.h
 * Author: spink
 *
 * Created on 23 February 2015, 08:08
 */

#ifndef VIRTUAL_SCREEN_H
#define	VIRTUAL_SCREEN_H

#include <define.h>

namespace captive {
	namespace devices {
		namespace gfx {
			class VirtualScreenConfiguration
			{
			public:
				enum VirtualScreenMode {
					VS_DUMMY,
					VS_8bpp,
					VS_16bpp,
					VS_32bpp,
					VS_Doom,
				};

				explicit VirtualScreenConfiguration(uint32_t width, uint32_t height, VirtualScreenMode mode) : _width(width), _height(height), _mode(mode) { }

				inline uint32_t width() const { return _width; }
				inline uint32_t height() const { return _height; }
				inline VirtualScreenMode mode() const { return _mode; }

			private:
				uint32_t _width, _height;
				VirtualScreenMode _mode;
			};

			class VirtualScreen
			{
			public:
				VirtualScreen();
				virtual ~VirtualScreen();

				virtual bool initialise() = 0;

				bool reset();
				bool configure(const VirtualScreenConfiguration& config);

				inline const VirtualScreenConfiguration& config() const { return *_config; }

				void framebuffer(uint8_t *fb) { _framebuffer = fb; }
				void palette(uint8_t *pp) { _palette = pp; }

				inline uint8_t *framebuffer() const { return _framebuffer; }
				inline uint8_t *palette() const { return _palette; }

				inline bool configured() const { return _config != NULL; }

			protected:
				virtual bool activate_configuration(const VirtualScreenConfiguration& cfg) = 0;
				virtual bool reset_configuration() = 0;

			private:
				const VirtualScreenConfiguration *_config;
				uint8_t *_framebuffer, *_palette;
			};
		}
	}
}

#endif	/* VIRTUAL_SCREEN_H */

