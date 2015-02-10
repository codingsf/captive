/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:51
 */

#ifndef CPU_H
#define	CPU_H

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

		private:
			Environment& _env;
		};
	}
}

#endif	/* CPU_H */

