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

#include <hypervisor/config.h>
#include <hypervisor/gpa-resolver.h>

namespace captive {
	namespace engine {
		class Engine;
	}

	namespace platform {
		class Platform;
	}

	namespace loader {
		class Loader;
	}

	namespace hypervisor {
		class Hypervisor;
		class CPU;

		class SharedMemory;

		class GuestConfiguration;
		class GuestCPUConfiguration;

		class Guest : public GPAResolver
		{
		public:
			static __thread CPU *current_core;
			
			typedef void (*memory_callback_fn_t)(gpa_t gpa);

			Guest(Hypervisor& owner, engine::Engine& engine, platform::Platform& pfm);
			virtual ~Guest();
			virtual bool init();

			virtual bool load(loader::Loader& loader) = 0;

			inline Hypervisor& owner() const { return _owner; }
			inline engine::Engine& engine() const { return _engine; }

			inline platform::Platform& platform() const { return _pfm; }

			virtual void guest_entrypoint(gpa_t ep) = 0;

			virtual bool resolve_gpa(gpa_t gpa, void*& out_addr) const = 0;
			
			virtual bool run() = 0;
			virtual void stop() = 0;
			
			virtual void debug_interrupt(int code) = 0;

		private:
			Hypervisor& _owner;
			engine::Engine& _engine;
			platform::Platform& _pfm;
		};
	}
}

#endif	/* GUEST_H */

