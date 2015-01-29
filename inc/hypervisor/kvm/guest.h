/*
 * File:   guest.h
 * Author: spink
 *
 * Created on 29 January 2015, 09:11
 */

#ifndef KVM_GUEST_H
#define	KVM_GUEST_H

#include <hypervisor/guest.h>

namespace captive {
	namespace hypervisor {
		namespace kvm {
			class KVM;
			class KVMCpu;

			class KVMGuest : public Guest {
			public:
				KVMGuest(KVM& owner, engine::Engine& engine, const GuestConfiguration& config, int fd);
				virtual ~KVMGuest();

				bool init() override;
				CPU *create_cpu(const GuestCPUConfiguration& config) override;

			private:
				int fd;
				int next_cpu_id;
				std::vector<KVMCpu *> kvm_cpus;
			};
		}
	}
}

#endif	/* GUEST_H */

