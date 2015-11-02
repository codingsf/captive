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
		class CPU;
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
			
			inline void add_core(hypervisor::CPU& core) { _cores.push_back(&core); }			
			inline const std::list<hypervisor::CPU *>& cores() const { return _cores; }
			
		private:
			std::list<hypervisor::CPU *> _cores;
		};
	}
}

#endif /* PLATFORM_H */

