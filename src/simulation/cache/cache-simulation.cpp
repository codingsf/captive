#include <simulation/cache/cache-simulation.h>

using namespace captive::simulation::cache;

bool CacheSimulation::init()
{
	/*l1d = new simulation::cache::CPUCache<32768, 64, 2>();
	l1i = new simulation::cache::CPUCache<32768, 64, 2>();
	l2 = new simulation::cache::CPUCache<1048576, 64, 16>();*/

	return true;
}

void CacheSimulation::start()
{
}

void CacheSimulation::stop()
{
}

void CacheSimulation::dump()
{
	fprintf(stderr, "*** CACHE STATISTICS ***\n");
	fprintf(stderr, "l1i: fetch: accesses=%lu, hits=%lu, misses=%lu\n", l1i.read_misses + l1i.read_hits, l1i.read_hits, l1i.read_misses);

	fprintf(stderr, "l1d:  read: accesses=%lu, hits=%lu, misses=%lu\n", l1d.read_misses + l1d.read_hits, l1d.read_hits, l1d.read_misses);
	fprintf(stderr, "l1d: write: accesses=%lu, hits=%lu, misses=%lu\n", l1d.write_misses + l1d.write_hits, l1d.write_hits, l1d.write_misses);
	
	fprintf(stderr, "l2:   read: accesses=%lu, hits=%lu, misses=%lu\n", l2.read_misses + l2.read_hits, l2.read_hits, l2.read_misses);
	fprintf(stderr, "l2:  write: accesses=%lu, hits=%lu, misses=%lu\n", l2.write_misses + l2.write_hits, l2.write_hits, l2.write_misses);
}


void CacheSimulation::process_events(const EventPacket *events, uint32_t count)
{
	for (uint32_t i = 0; i < count; i++) {
		if (events[i].type == EventPacket::MEMORY_READ) {
			if (!l1d.read(events[i].address)) {
				l2.read(events[i].address);
			}
		} else if (events[i].type == EventPacket::MEMORY_WRITE) {
			if (!l1d.write(events[i].address)) {
				l2.write(events[i].address);
			}
		} else if (events[i].type == EventPacket::INSTRUCTION_FETCH) {
			if (!l1i.read(events[i].address)) {
				l2.read(events[i].address);
			}
		}
	}
}
