/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:51
 */

#ifndef CPU_H
#define	CPU_H

#include <define.h>

namespace captive {
	namespace arch {
		class Environment;
		class MMU;

		class CPU
		{
		public:
			CPU(Environment& env);
			virtual ~CPU();

			virtual bool init(unsigned int ep) = 0;
			virtual bool run() = 0;

			virtual uint32_t read_pc() const = 0;
			virtual uint32_t write_pc(uint32_t new_pc_val) = 0;
			virtual uint32_t inc_pc(uint32_t delta) = 0;

			virtual void dump_state() const = 0;

			inline Environment& env() const { return _env; }
			virtual MMU& mmu() const = 0;

			inline uint64_t get_insns_executed() const { return insns_executed; }

		protected:
			inline void inc_insns_executed() {
				insns_executed++;
			}

		private:
			uint64_t insns_executed;
			Environment& _env;
		};

		extern CPU *active_cpu;
	}
}

#endif	/* CPU_H */

