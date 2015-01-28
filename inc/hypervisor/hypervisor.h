/* 
 * File:   hypervisor.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 14:46
 */

#ifndef HYPERVISOR_H
#define	HYPERVISOR_H

#include "hypervisor/guest.h"

namespace captive {
	namespace hypervisor {
		class Guest;

		class Hypervisor {
		public:
			Hypervisor();
			virtual ~Hypervisor();

			virtual Guest *create_guest() = 0;
		};
	}
}

#endif	/* HYPERVISOR_H */

