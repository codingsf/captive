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
			template<uint32_t total_size, uint32_t line_size, uint8_t ways, bool vidx, bool vtag, bool nonseq>
			struct CPUCache
			{
				public:
					CPUCache() : read_hits(0), read_misses(0), write_hits(0), write_misses(0), rrp(0x9E3779B9), last_line(-1) { }
					
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
				uint32_t last_line;
				
				inline uint32_t cache_line(uint32_t vaddr, uint32_t paddr) const __attribute__((pure))
				{
					return index_of(vaddr, paddr);
				}
				
				inline constexpr uint32_t tag_addr(uint32_t vaddr, uint32_t paddr) const __attribute__((pure))
				{
					return vtag ? vaddr : paddr;
				}
						
				inline constexpr uint32_t index_addr(uint32_t vaddr, uint32_t paddr) const __attribute__((pure))
				{
					return vidx ? vaddr : paddr;
				}
						
				inline constexpr uint32_t tag_of(uint32_t vaddr, uint32_t paddr) const __attribute__((pure))
				{
					return (tag_addr(vaddr, paddr) >> (index_bits::value + offset_bits::value)) & tag_bits::mask;
				}
				
				inline constexpr uint32_t index_of(uint32_t vaddr, uint32_t paddr) const __attribute__((pure))
				{
					return (index_addr(vaddr, paddr) >> offset_bits::value) & index_bits::mask;
				}
				
				inline uint32_t tag_at(uint32_t vaddr, uint32_t paddr, uint8_t way) const
				{				
					return tags[index_of(vaddr, paddr) + number_of_lines::value * way];
				}
				
				inline void tag_set(uint32_t vaddr, uint32_t paddr, uint8_t way)
				{				
					tags[index_of(vaddr, paddr) + number_of_lines::value * way] = tag_of(vaddr, paddr);
				}
				
				inline bool hit(uint32_t vaddr, uint32_t paddr) const
				{
					if (ways == 2) {
						return tag_at(vaddr, paddr, 0) == tag_of(vaddr, paddr) || 
								tag_at(vaddr, paddr, 1) == tag_of(vaddr, paddr);
					} else if (ways == 4) {
						return tag_at(vaddr, paddr, 0) == tag_of(vaddr, paddr) || 
								tag_at(vaddr, paddr, 1) == tag_of(vaddr, paddr) ||
								tag_at(vaddr, paddr, 2) == tag_of(vaddr, paddr) ||
								tag_at(vaddr, paddr, 3) == tag_of(vaddr, paddr);
					}
				}
				
				inline void replace(uint32_t vaddr, uint32_t paddr)
				{
					if (ways == 2) tag_set(vaddr, paddr, rrp & 1);
					else if (ways == 4) tag_set(vaddr, paddr, rrp & 2);
					
					rrp = ((rrp & 1) << 31) | (rrp >> 1);
				}
				
				inline bool read(uint32_t vaddr, uint32_t paddr, uint8_t sz)
				{
					if (hit(vaddr, paddr)) {
						if (nonseq) {
							if (cache_line(vaddr, paddr) == last_line) return true;
							last_line = cache_line(vaddr, paddr);
						}
						
						read_hits++;
						return true;
					} else {
						read_misses++;
						replace(vaddr, paddr);
						return false;
					}
				}
				
				inline bool write(uint32_t vaddr, uint32_t paddr, uint8_t sz)
				{
					if (hit(vaddr, paddr)) {
						if (nonseq) {
							if (cache_line(vaddr, paddr) == last_line) return true;
							last_line = cache_line(vaddr, paddr);
						}
						
						write_hits++;
						return true;
					} else {
						write_misses++;
						replace(vaddr, paddr);
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
				
				void begin_record() override;
				void end_record() override;
				
			private:
				simulation::cache::CPUCache<32768, 32, 2, true, true, false> l1i;
				simulation::cache::CPUCache<32768, 64, 4, true, true, true> l1d;
				
				uint64_t l1d_read_hits, l1d_read_misses, l1d_write_hits, l1d_write_misses;
				uint64_t l1i_fetch_hits, l1i_fetch_misses;
			};
		}
	}
}

#endif /* CACHE_SIMULATION_H */

