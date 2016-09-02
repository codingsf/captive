#define XXX
#include <simulation/cache/cache-simulation.h>

using namespace captive::simulation::cache;

void CacheSimulation::instruction_fetch(hypervisor::CPU& core, uint32_t virt_pc, uint32_t phys_pc)
{
	d4memref memref;
	memref.address = (d4addr)virt_pc;
	memref.size = 4;
	memref.accesstype = D4XINSTRN;

	d4ref(l1i, memref);
}

bool CacheSimulation::init() {
	mm = d4new(NULL);
	mm->name = "memory";
	
	l2 = d4new(mm);
	l2->name = "l2";
	l2->flags = 0;
		
	l2->lg2blocksize = 6;
	l2->lg2subblocksize = 6;
	l2->lg2size = 20;
	l2->assoc = 8;
	
	l2->replacementf = d4rep_lru;
	l2->name_replacement = "LRU";
	
	l2->prefetchf = d4prefetch_none;
	l2->name_prefetch = "demand only";
	
	l2->wallocf = d4walloc_never;
	l2->name_walloc = "never";
	
	l2->wbackf = d4wback_never;
	l2->name_wback = "never";
	
	l2->prefetch_distance = 6;
	l2->prefetch_abortpercent = 0;
	
	l1d = d4new(l2);
	l1d->name = "l1d";
	l1d->flags = 0;
		
	l1d->lg2blocksize = 6;
	l1d->lg2subblocksize = 6;
	l1d->lg2size = 15;
	l1d->assoc = 4;
	
	l1d->replacementf = d4rep_random;
	l1d->name_replacement = "random";
	
	l1d->prefetchf = d4prefetch_none;
	l1d->name_prefetch = "demand only";
	
	l1d->wallocf = d4walloc_never;
	l1d->name_walloc = "never";
	
	l1d->wbackf = d4wback_never;
	l1d->name_wback = "never";
	
	l1d->prefetch_distance = 6;
	l1d->prefetch_abortpercent = 0;
	
	l1i = d4new(l2);
	l1i->name = "l1i";
	l1i->flags = D4F_RO;
		
	l1i->lg2blocksize = 6;
	l1i->lg2subblocksize = 6;
	l1i->lg2size = 15;
	l1i->assoc = 4;
	
	l1i->replacementf = d4rep_random;
	l1i->name_replacement = "random";
	
	l1i->prefetchf = d4prefetch_none;
	l1i->name_prefetch = "demand only";
	
	l1i->wallocf = d4walloc_never;
	l1i->name_walloc = "never";
	
	l1i->wbackf = d4wback_never;
	l1i->name_wback = "never";
	
	l1i->prefetch_distance = 6;
	l1i->prefetch_abortpercent = 0;
	
	int err = d4setup();
	
	if (err) {
		fprintf(stderr, "ERROR: %d\n", err);
		return false;
	}
	
	return true;
}

void CacheSimulation::start()
{

}

void CacheSimulation::stop()
{
	fprintf(stderr, "*** CACHE STATISTICS ***\n");
	fprintf(stderr, "l1i: fetch:  accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)l1i->fetch[D4XINSTRN], (uint64_t)l1i->fetch[D4XINSTRN] - (uint64_t)l1i->miss[D4XINSTRN], (uint64_t)l1i->miss[D4XINSTRN]);
	
	fprintf(stderr, "l1d: read:  accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)l1d->fetch[D4XREAD], (uint64_t)l1d->fetch[D4XREAD] - (uint64_t)l1d->miss[D4XREAD], (uint64_t)l1d->miss[D4XREAD]);
	fprintf(stderr, "l1d: write: accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)l1d->fetch[D4XWRITE], (uint64_t)l1d->fetch[D4XWRITE] - (uint64_t)l1d->miss[D4XWRITE], (uint64_t)l1d->miss[D4XWRITE]);

	fprintf(stderr, "l2:  read:  accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)l2->fetch[D4XREAD], (uint64_t)l2->fetch[D4XREAD] - (uint64_t)l2->miss[D4XREAD], (uint64_t)l2->miss[D4XREAD]);
	fprintf(stderr, "l2:  write: accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)l2->fetch[D4XWRITE], (uint64_t)l2->fetch[D4XWRITE] - (uint64_t)l2->miss[D4XWRITE], (uint64_t)l2->miss[D4XWRITE]);
	
	fprintf(stderr, "mem: read:  accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)mm->fetch[D4XREAD], (uint64_t)mm->fetch[D4XREAD] - (uint64_t)mm->miss[D4XREAD], (uint64_t)mm->miss[D4XREAD]);
	fprintf(stderr, "mem: write: accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)mm->fetch[D4XWRITE], (uint64_t)mm->fetch[D4XWRITE] - (uint64_t)mm->miss[D4XWRITE], (uint64_t)mm->miss[D4XWRITE]);
	
	//fprintf(stderr, "*** INSTRUCTION COUNT: %lu (%lu)\n", core->per_cpu_data().insns_executed, tmp);	
}

#if 0

	
	volatile uint64_t *ring = (volatile uint64_t *)core->per_cpu_data().event_ring;

	uint64_t tmp = 0;
	
	uint64_t last = ring[0];
	while (0) { //!core->per_cpu_data().halt) {
		uint64_t next = ring[0];
		
		if (next != last) {
			for (; last != next; last++, last &= 0xfff) {
				uint64_t entry = ring[last];
				
				if ((entry & 0x80000000)) {
					fprintf(stderr, "*** CACHE STATISTICS ***\n");
					fprintf(stderr, "l1i: fetch:  accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)l1i->fetch[D4XINSTRN], (uint64_t)l1i->fetch[D4XINSTRN] - (uint64_t)l1i->miss[D4XINSTRN], (uint64_t)l1i->miss[D4XINSTRN]);

					fprintf(stderr, "l1d: read:  accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)l1d->fetch[D4XREAD], (uint64_t)l1d->fetch[D4XREAD] - (uint64_t)l1d->miss[D4XREAD], (uint64_t)l1d->miss[D4XREAD]);
					fprintf(stderr, "l1d: write: accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)l1d->fetch[D4XWRITE], (uint64_t)l1d->fetch[D4XWRITE] - (uint64_t)l1d->miss[D4XWRITE], (uint64_t)l1d->miss[D4XWRITE]);

					fprintf(stderr, "l2:  read:  accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)l2->fetch[D4XREAD], (uint64_t)l2->fetch[D4XREAD] - (uint64_t)l2->miss[D4XREAD], (uint64_t)l2->miss[D4XREAD]);
					fprintf(stderr, "l2:  write: accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)l2->fetch[D4XWRITE], (uint64_t)l2->fetch[D4XWRITE] - (uint64_t)l2->miss[D4XWRITE], (uint64_t)l2->miss[D4XWRITE]);

					fprintf(stderr, "mem: read:  accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)mm->fetch[D4XREAD], (uint64_t)mm->fetch[D4XREAD] - (uint64_t)mm->miss[D4XREAD], (uint64_t)mm->miss[D4XREAD]);
					fprintf(stderr, "mem: write: accesses=%lu, hits=%lu, misses=%lu\n", (uint64_t)mm->fetch[D4XWRITE], (uint64_t)mm->fetch[D4XWRITE] - (uint64_t)mm->miss[D4XWRITE], (uint64_t)mm->miss[D4XWRITE]);
				} else {
					d4memref memref;
					memref.address = (d4addr)(entry >> 32);
					memref.size = (entry & 0xf);
					
					switch (entry & 0x30) {
					case 0x00: memref.accesstype = D4XREAD;		d4ref(l1d, memref); break;
					case 0x10: memref.accesstype = D4XWRITE;	d4ref(l1d, memref); break;
					case 0x20: memref.accesstype = D4XINSTRN;	tmp++; d4ref(l1i, memref); break;
					}
				}
			}
		}
	}
#endif
	