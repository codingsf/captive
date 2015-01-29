/*
 * File:   guest.h
 * Author: spink
 *
 * Created on 29 January 2015, 09:11
 */

#ifndef KVM_GUEST_H
#define	KVM_GUEST_H

#include <hypervisor/guest.h>
#include <linux/kvm.h>

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

				inline bool initialised() const { return _initialised; }

			private:
				bool _initialised;
				int fd;
				int next_cpu_id;
				std::vector<KVMCpu *> kvm_cpus;

				struct vm_mem_region {
					struct kvm_userspace_memory_region kvm;

					uint64_t buffer;
					uint64_t buffer_size;
				};

				std::vector<struct vm_mem_region> vm_mem_regions;

				bool prepare_guest_memory();
			};
		}
	}
}

#endif	/* GUEST_H */

