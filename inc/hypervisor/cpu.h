/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 29 January 2015, 08:46
 */

#ifndef CPU_H
#define	CPU_H

#include <define.h>
#include <shmem.h>

namespace captive {
	namespace hypervisor {
		class Guest;
		class GuestCPUConfiguration;

		class CPU
		{
		public:
			CPU(Guest& owner, const GuestCPUConfiguration& config, PerCPUData *per_cpu_data);
			virtual ~CPU();

			virtual bool init();
			virtual bool run() = 0;
			virtual void stop() = 0;

			inline Guest& owner() const { return _owner; }
			inline const GuestCPUConfiguration& config() const { return _config; }
			inline PerCPUData& per_cpu_data() const { return *_per_cpu_data; }

			virtual void interrupt(uint32_t code) = 0;

		private:
			Guest& _owner;
			const GuestCPUConfiguration& _config;
			PerCPUData *_per_cpu_data;
		};
	}
}

#endif	/* CPU_H */

