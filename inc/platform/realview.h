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

namespace captive {
	namespace devices {
		namespace arm {
			class PL011;
		}
		
		namespace gfx {
			class SDLVirtualScreen;
		}
		
		namespace timers {
			class TickSource;
		}
		
		namespace io {
			class SocketUART;
		}
	}
	
	namespace platform {
		class Realview : public Platform
		{
		public:
			Realview(devices::timers::TickSource& ts, std::string block_device_file);
			virtual ~Realview();
			
			const hypervisor::GuestConfiguration& config() const override;
			
			bool start() override;
			bool stop() override;
			
		private:
			hypervisor::GuestConfiguration cfg;
			devices::arm::PL011 *uart0, *uart1, *uart2, *uart3;
			devices::gfx::SDLVirtualScreen *vs;
			devices::io::SocketUART *socket_uart;
		};
	}
}

#endif /* REALVIEW_H */

