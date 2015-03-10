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
	struct shmem_data;

	namespace engine {
		class Engine;
	}

	namespace jit {
		class JIT;
	}

	namespace loader {
		class Loader;
	}

	namespace hypervisor {
		class Hypervisor;
		class CPU;

		class GuestConfiguration;
		class GuestCPUConfiguration;

		class Guest : public GPAResolver
		{
		public:
			Guest(Hypervisor& owner, engine::Engine& engine, jit::JIT& jit, const GuestConfiguration& config);
			virtual ~Guest();
			virtual bool init();

			virtual bool load(loader::Loader& loader) = 0;

			virtual CPU *create_cpu(const GuestCPUConfiguration& config) = 0;

			inline Hypervisor& owner() const { return _owner; }
			inline engine::Engine& engine() const { return _engine; }
			inline jit::JIT& jit() const { return _jit; }

			inline const GuestConfiguration& config() const { return _config; }

			inline gpa_t guest_entrypoint() const { return _guest_entrypoint; }
			inline void guest_entrypoint(gpa_t ep) { _guest_entrypoint = ep; }

			virtual bool resolve_gpa(gpa_t gpa, void*& out_addr) const = 0;

		private:
			Hypervisor& _owner;
			engine::Engine& _engine;
			jit::JIT& _jit;
			const GuestConfiguration& _config;

			gpa_t _guest_entrypoint;
		};
	}
}

#endif	/* GUEST_H */

