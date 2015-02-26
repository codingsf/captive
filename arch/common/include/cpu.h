/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:51
 */

#ifndef CPU_H
#define	CPU_H

#include <define.h>
#include <trace.h>
#include <mmu.h>

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

			inline Trace& trace() { return *_trace; }

			inline bool kernel_mode() const { return _kernel_mode; }

			inline void kernel_mode(bool km) {
				if (_kernel_mode != km) {
					_kernel_mode = km;
					mmu().cpu_privilege_change(km);
				}
			}

		protected:
			inline void inc_insns_executed() {
				insns_executed++;
			}

			Trace *_trace;

		private:
			bool _kernel_mode;

			uint64_t insns_executed;
			Environment& _env;
		};

		extern CPU *active_cpu;
	}
}

#endif	/* CPU_H */

