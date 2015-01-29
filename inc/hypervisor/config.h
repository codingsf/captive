/*
 * File:   config.h
 * Author: spink
 *
 * Created on 29 January 2015, 08:49
 */

#ifndef CONFIG_H
#define	CONFIG_H

#include <define.h>

namespace captive {
	namespace hypervisor {
		class GuestCPUConfiguration {
		public:
			explicit GuestCPUConfiguration() { }

			bool validate() const;
		};

		class GuestMemoryRegionConfiguration
		{
		public:
			explicit GuestMemoryRegionConfiguration(uint64_t base_address, uint64_t size) : _base_address(base_address), _size(size) { }

			inline uint64_t base_address() const { return _base_address; }
			inline uint64_t size() const { return _size; }

			bool validate() const;

		private:
			uint64_t _base_address;
			uint64_t _size;
		};

		class GuestConfiguration
		{
		public:
			std::string name;
			std::vector<GuestMemoryRegionConfiguration> memory_regions;

			inline bool have_memory_regions() const { return !memory_regions.empty(); }

			bool validate() const;
		};
	}
}

#endif	/* CONFIG_H */

