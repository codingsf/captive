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
	namespace devices {
		class Device;
	}

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

		class GuestDeviceConfiguration
		{
		public:
			explicit GuestDeviceConfiguration(uint64_t base_address, uint64_t size, devices::Device& dev)
				: _base_address(base_address), _size(size), _dev(dev) { }

			inline uint64_t base_address() const { return _base_address; }
			inline uint64_t size() const { return _size; }
			inline devices::Device& device() const { return _dev; }

		private:
			uint64_t _base_address;
			uint64_t _size;
			devices::Device& _dev;
		};

		class GuestConfiguration
		{
		public:
			std::string name;
			std::vector<GuestMemoryRegionConfiguration> memory_regions;
			std::vector<GuestDeviceConfiguration> devices;

			inline bool have_memory_regions() const { return !memory_regions.empty(); }
			inline bool have_devices() const { return !devices.empty(); }

			bool validate() const;
		};
	}
}

#endif	/* CONFIG_H */

