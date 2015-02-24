/*
 * File:   sdl-virtual-screen.h
 * Author: spink
 *
 * Created on 23 February 2015, 18:11
 */

#ifndef SDL_VIRTUAL_SCREEN_H
#define	SDL_VIRTUAL_SCREEN_H

#include <devices/gfx/virtual-screen.h>
#include <mutex>
#include <thread>

#include <SDL2/SDL.h>

namespace captive {
	namespace hypervisor {
		class Guest;
	}

	namespace devices {
		namespace gfx {
			class SDLVirtualScreen : public VirtualScreen
			{
			public:
				SDLVirtualScreen();
				virtual ~SDLVirtualScreen();

				bool initialise() override;

				inline void guest(hypervisor::Guest& guest) {
					_guest = &guest;
				}

			protected:
				bool activate_configuration(const VirtualScreenConfiguration& cfg) override;
				bool reset_configuration() override;

			private:
				static std::mutex _sdl_lock;
				static bool _sdl_initialised;

				static void window_thread_proc_tramp(SDLVirtualScreen *o);
				void window_thread_proc();
				void draw_frame();

				void draw_doom();
				void draw_rgb();

				typedef void (SDLVirtualScreen::*frame_drawer_fn_t)(void);
				frame_drawer_fn_t frame_drawer;

				hypervisor::Guest *_guest;

				bool hw_accelerated, terminate;

				int _sdl_mode;
				uint32_t pitch;

				SDL_Window *window;
				SDL_Renderer *renderer;
				SDL_Texture *window_texture;

				std::thread *window_thread;
			};
		}
	}
}

#endif	/* SDL_VIRTUAL_SCREEN_H */

