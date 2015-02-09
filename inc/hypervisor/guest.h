/*
 * File:   guest.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 14:52
 */

#ifndef GUEST_H
#define	GUEST_H

#include <define.h>
#include <vector>

#include "config.h"

namespace captive {
	namespace engine {
		class Engine;
	}

	namespace loader {
		class Loader;
	}

	namespace hypervisor {
		class Hypervisor;
		class CPU;

		class GuestConfiguration;
		class GuestCPUConfiguration;

		class Guest
		{
		public:
			Guest(Hypervisor& owner, engine::Engine& engine, const GuestConfiguration& config);
			virtual ~Guest();
			virtual bool init();

			virtual bool load(loader::Loader& loader) = 0;

			virtual CPU *create_cpu(const GuestCPUConfiguration& config) = 0;

			inline Hypervisor& owner() const { return _owner; }
			inline engine::Engine& engine() const { return _engine; }

			inline const GuestConfiguration& config() const { return _config; }

		private:
			Hypervisor& _owner;
			engine::Engine& _engine;
			const GuestConfiguration& _config;
		};
	}
}

#endif	/* GUEST_H */

