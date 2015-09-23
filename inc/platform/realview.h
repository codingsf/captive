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

namespace captive {
	namespace devices {
		namespace arm {
			class PL011;
		}
		
		namespace timers {
			class TickSource;
		}
	}
	
	namespace platform {
		class Realview : public Platform
		{
		public:
			Realview(devices::timers::TickSource& ts);
			virtual ~Realview();
			
			const hypervisor::GuestConfiguration& config() const override;
			
			bool start() override;
			bool stop() override;
			
		private:
			hypervisor::GuestConfiguration cfg;
			devices::arm::PL011 *uart0;
		};
	}
}

#endif /* REALVIEW_H */

