/* 
 * File:   platform.h
 * Author: s0457958
 *
 * Created on 22 September 2015, 12:21
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <list>

namespace captive {
	namespace hypervisor {
		class GuestConfiguration;
	}
	
	namespace platform {
		class Platform
		{
		public:
			Platform();
			virtual ~Platform();
			
			virtual const hypervisor::GuestConfiguration& config() const = 0;
			
			virtual bool start() = 0;
			virtual bool stop() = 0;			
		};
	}
}

#endif /* PLATFORM_H */

