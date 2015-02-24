/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 29 January 2015, 08:46
 */

#ifndef CPU_H
#define	CPU_H

#include <define.h>

namespace captive {
	namespace hypervisor {
		class Guest;
		class GuestCPUConfiguration;

		class CPU
		{
		public:
			CPU(Guest& owner, const GuestCPUConfiguration& config);
			virtual ~CPU();

			virtual bool init();
			virtual bool run() = 0;
			virtual void stop() = 0;

			inline Guest& owner() const { return _owner; }
			inline const GuestCPUConfiguration& config() const { return _config; }

			virtual void interrupt(uint32_t code) = 0;

		private:
			Guest& _owner;
			const GuestCPUConfiguration& _config;
		};
	}
}

#endif	/* CPU_H */

