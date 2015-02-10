/*
 * File:   env.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:42
 */

#ifndef ENV_H
#define	ENV_H

namespace captive {
	namespace arch {
		class CPU;

		class Environment
		{
		public:
			Environment();
			virtual ~Environment();

			bool run(unsigned int ep);

			virtual CPU *create_cpu() = 0;
		};
	}
}

#endif	/* ENV_H */

