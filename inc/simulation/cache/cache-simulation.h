/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   cache-simulation.h
 * Author: s0457958
 *
 * Created on 01 September 2016, 16:13
 */

#ifndef CACHE_SIMULATION_H
#define CACHE_SIMULATION_H

#include <simulation/simulation.h>
#include <cmath>

namespace captive
{
	namespace simulation
	{
		namespace cache
		{
			template<uint32_t total_size, uint32_t line_size, uint8_t ways>
			struct CPUCache
			{
				public:
					CPUCache() : read_hits(0), read_misses(0), write_hits(0), write_misses(0), rrp(0x9E3779B9) { }
					
				struct number_of_lines
				{
					enum { value = (int)(total_size / line_size) };
					enum { lg2 = (int)std::log2(total_size / line_size) };
				};
				
				struct offset_bits
				{
					enum { value = (int)std::log2(line_size), mask = ((1 << (int)std::log2(line_size)) - 1) };
				};
				
				struct index_bits
				{
					enum { value = number_of_lines::lg2, mask = ((1 << number_of_lines::lg2) - 1) };
				};
				
				struct tag_bits
				{
					enum
					{
						value = 31 - index_bits::value - offset_bits::value,
						mask = ((1 << value) - 1),
					};
				};
								
				uint64_t read_hits, read_misses, write_hits, write_misses;
				uint32_t tags[number_of_lines::value * ways];
				uint32_t rrp;
						
				inline constexpr uint32_t tag_of(uint32_t address) const __attribute__((pure))
				{
					return (address >> (index_bits::value + offset_bits::value)) & tag_bits::mask;
				}
				
				inline constexpr uint32_t index_of(uint32_t address) const __attribute__((pure))
				{
					return (address >> offset_bits::value) & index_bits::mask;
				}
				
				inline uint32_t tag_at(uint32_t address, uint8_t way) const
				{				
					return tags[index_of(address) + number_of_lines::value * way];
				}
				
				inline void tag_set(uint32_t address, uint8_t way)
				{				
					tags[index_of(address) + number_of_lines::value * way] = tag_of(address);
				}
				
				inline bool read(uint32_t address)
				{
					if (tag_at(address, 0) == tag_of(address) || tag_at(address, 1) == tag_of(address)) {
						read_hits++;
						return true;
					} else {
						read_misses++;
						tag_set(address, rrp & 1);
						rrp = ((rrp & 1) << 31) | (rrp >> 1);
						
						return false;
					}
				}
				
				inline bool write(uint32_t address)
				{
					if (tag_at(address, 0) == tag_of(address) || tag_at(address, 1) == tag_of(address)) {
						write_hits++;
						return true;
					} else {
						write_misses++;
						tag_set(address, rrp & 1);
						rrp = ((rrp & 1) << 31) | (rrp >> 1);
						
						return false;
					}
				}
			};
			
			class CacheSimulation : public Simulation
			{
			public:
				bool init() override;
				void start() override;
				void stop() override;
				
				void process_events(const EventPacket *events, uint32_t count) override;
								
				Events::Events required_events() const override { return (Events::Events)(Events::InstructionFetch | Events::MemoryRead | Events::MemoryWrite); }
				
				void dump() override;
				
			private:
				simulation::cache::CPUCache<32768, 64, 2> l1i, l1d;
				simulation::cache::CPUCache<1048576, 64, 16> l2;
			};
		}
	}
}

#endif /* CACHE_SIMULATION_H */

