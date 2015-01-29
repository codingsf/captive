/* 
 * File:   guest.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 14:52
 */

#ifndef GUEST_H
#define	GUEST_H

#include <define.h>
#include <vector>

namespace captive {
	namespace engine {
		class Engine;
	}
	
	namespace hypervisor {
		class Hypervisor;

		class GuestMemoryRegion
		{
		public:
			explicit GuestMemoryRegion(uint64_t base_address, uint64_t size) : _base_address(base_address), _size(size) { }
			
			inline uint64_t base_address() const { return _base_address; }
			inline uint64_t size() const { return _size; }
			
		private:
			uint64_t _base_address;
			uint64_t _size;
		};
		
		class GuestCPU {
		public:
			explicit GuestCPU();
		};
		
		class GuestConfiguration
		{
		public:
			std::string name;
			std::vector<GuestCPU> cpus;
			std::vector<GuestMemoryRegion> memory_regions;
			
			inline bool have_cpus() const { return !cpus.empty(); }
			inline bool have_memory_regions() const { return !memory_regions.empty(); }
			
			bool validate() const;
		};
		
		class Guest
		{
		public:
			Guest(Hypervisor& owner, const GuestConfiguration& config);
			virtual ~Guest();
			virtual bool start(engine::Engine& engine) = 0;
			
			inline Hypervisor& owner() const { return _owner; }
			inline const GuestConfiguration& config() const { return _config; }
			
		private:
			Hypervisor& _owner;
			const GuestConfiguration& _config;
		};
	}
}

#endif	/* GUEST_H */

