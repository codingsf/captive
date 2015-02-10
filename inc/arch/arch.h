/*
 * File:   arch.h
 * Author: spink
 *
 * Created on 10 February 2015, 09:11
 */

#ifndef ARCH_H
#define	ARCH_H

namespace captive {
	namespace arch {
		class CpuEnvironment;
		class SystemEnvironment;

		class Architecture
		{
		public:
			Architecture();
			virtual ~Architecture();
			
			virtual SystemEnvironment *create_system() = 0;
		};
	}
}

#endif	/* ARCH_H */
