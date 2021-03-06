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
	namespace engine {
		class Engine;
	}

	namespace platform {
		class Platform;
	}

	namespace hypervisor {
		class Guest;

		class Hypervisor {
		public:
			Hypervisor();
			virtual ~Hypervisor();

			virtual bool init();
			virtual Guest *create_guest(engine::Engine& engine, platform::Platform& platform) = 0;
		};
	}
}

#endif	/* HYPERVISOR_H */

