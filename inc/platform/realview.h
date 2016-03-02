/* 
 * File:   realview.h
 * Author: s0457958
 *
 * Created on 22 September 2015, 12:22
 */

#ifndef REALVIEW_H
#define REALVIEW_H

#include <platform/platform.h>
#include <hypervisor/config.h>
#include <string>
#include <list>

namespace captive {
	namespace devices {
		namespace arm {
			class PL011;
		}
		
		namespace gfx {
			class NullVirtualScreen;
			class SDLVirtualScreen;
		}
				
		namespace io {
			class SocketUART;
		}
	}
	
	namespace platform {
		class Realview : public Platform
		{
		public:
			enum Variant
			{
				CORTEX_A8,
				CORTEX_A9,
			};
			
			Realview(devices::timers::TimerManager& timer_manager, Variant variant, std::string block_device_file);
			virtual ~Realview();
			
			const hypervisor::GuestConfiguration& config() const override;
			
			bool start() override;
			bool stop() override;
			
		private:
			Variant variant;
			hypervisor::GuestConfiguration cfg;
			devices::arm::PL011 *uart0, *uart1, *uart2, *uart3;
#ifdef NULL_VIRTUAL_SCREEN
			devices::gfx::NullVirtualScreen *vs;
#else
			devices::gfx::SDLVirtualScreen *vs;
#endif
			devices::io::SocketUART *socket_uart;
			
			std::list<devices::Device *> destructor_list;
		};
	}
}

#endif /* REALVIEW_H */

