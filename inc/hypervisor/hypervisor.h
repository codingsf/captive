/* 
 * File:   hypervisor.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 14:46
 */

#ifndef HYPERVISOR_H
#define	HYPERVISOR_H

namespace captive {
	namespace hypervisor {
		class Guest;

		class Hypervisor {
		public:
			Hypervisor();
			virtual ~Hypervisor();

			virtual void run_guest(Guest& guest) = 0;
		};
	}
}

#endif	/* HYPERVISOR_H */

