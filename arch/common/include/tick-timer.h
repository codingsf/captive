#ifndef TICK_TIMER_H
#define TICK_TIMER_H

#include <vector>

class tick_timer
{
	public:
		tick_timer(bool enabled = 1) : enabled(enabled) {}
	
		void reset() { if(!enabled) return; ticks.clear(); }
		void tick() { if(!enabled) return; ticks.push_back(__rdtsc()); }
		
		void dump() 
		{
			if(!enabled) return;
			uint64_t last = ticks[0];
			for(uint32_t i = 1; i < ticks.size(); ++i) {
				uint64_t current = ticks[i];
				printf("%lu ", current-last);
				last = current;
			}
			
			printf("\n");
		}
		
	private:
		std::vector<uint64_t> ticks;
		bool enabled;
};

#endif
