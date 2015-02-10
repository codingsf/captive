/*
 * File:   guest.h
 * Author: spink
 *
 * Created on 29 January 2015, 09:11
 */

#ifndef KVM_GUEST_H
#define	KVM_GUEST_H

#include <list>

#include <hypervisor/guest.h>
#include <linux/kvm.h>

namespace captive {
	namespace arch {
		class SystemEnvironment;
	}

	namespace hypervisor {
		namespace kvm {
			class KVM;
			class KVMCpu;

			class KVMGuest : public Guest {
			public:
				KVMGuest(KVM& owner, engine::Engine& engine, arch::SystemEnvironment& sys_env, const GuestConfiguration& config, int fd);
				virtual ~KVMGuest();

				bool init() override;
				bool load(loader::Loader& loader) override;

				CPU *create_cpu(const GuestCPUConfiguration& config) override;

				inline bool initialised() const { return _initialised; }
				inline gpa_t guest_entrypoint() const { return _guest_entrypoint; }

				bool stage2_init();

			private:
				arch::SystemEnvironment& _sys_env;
				std::vector<KVMCpu *> kvm_cpus;

				bool _initialised;
				int fd;
				int next_cpu_id;
				int next_slot_idx;

				gpa_t _guest_entrypoint;

				struct vm_mem_region {
					struct kvm_userspace_memory_region kvm;
					void *host_buffer;
				};

				vm_mem_region *sys_mem_rgn;

				std::list<vm_mem_region *> vm_mem_region_free;
				std::list<vm_mem_region *> vm_mem_region_used;
				std::list<vm_mem_region *> gpm;

				bool prepare_guest_memory();
				bool install_bios();
				bool install_initial_pgt();

				vm_mem_region *get_mem_slot();
				void put_mem_slot(vm_mem_region *region);

				vm_mem_region *alloc_guest_memory(uint64_t gpa, uint64_t size);
				void release_guest_memory(vm_mem_region *rgn);
				void release_all_guest_memory();

				/*struct sys_page {
					gpa_t gpa;
					void *hva;
				};

				sys_page alloc_sys_page();*/
			};
		}
	}
}

#endif	/* GUEST_H */

