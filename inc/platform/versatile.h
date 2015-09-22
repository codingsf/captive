/* 
 * File:   versatile.h
 * Author: s0457958
 *
 * Created on 22 September 2015, 12:22
 */

#ifndef VERSATILE_H
#define VERSATILE_H

#include <platform/platform.h>

namespace captive {
	namespace platform {
		class VersatileAB : public Platform
		{
		public:
			const hypervisor::GuestConfiguration& config() const override;
			
			bool start() override;
			bool stop() override;
		};
	}
}

#endif /* VERSATILE_H */

