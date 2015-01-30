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
				int next_slot_idx;
				std::vector<KVMCpu *> kvm_cpus;

				struct vm_mem_region {
					struct kvm_userspace_memory_region kvm;
					void *host_buffer;
				};

				std::vector<struct vm_mem_region *> vm_mem_regions;

				bool prepare_guest_memory();
				bool prepare_bootstrap(uint8_t *base);

				const vm_mem_region *alloc_guest_memory(uint64_t gpa, uint64_t size);
				void release_guest_memory(const vm_mem_region *rgn);
				void release_all_guest_memory();
			};
		}
	}
}

#endif	/* GUEST_H */

