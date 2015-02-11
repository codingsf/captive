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

		class CPU
		{
		public:
			CPU(Environment& env);
			virtual ~CPU();

			virtual bool init(unsigned int ep) = 0;
			virtual bool run() = 0;

			virtual uint32_t read_pc() const = 0;
			virtual void dump_state() const = 0;

		private:
			Environment& _env;
		};
	}
}

#endif	/* CPU_H */

