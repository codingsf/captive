/*
 * File:   guest.h
 * Author: spink
 *
 * Created on 29 January 2015, 09:11
 */

#ifndef KVM_GUEST_H
#define	KVM_GUEST_H

#include <list>
#include <map>

#include <sys/ioctl.h>

#include <define.h>
#include <shmem.h>

#include <hypervisor/guest.h>
#include <hypervisor/shared-memory.h>
#include <util/spin-lock.h>

#include <linux/kvm.h>

extern void MMIOThreadTrampoline(void *);

namespace captive {
	namespace devices {
		class Device;
	}

	namespace hypervisor {
		namespace kvm {
			class KVM;
			class KVMCpu;

			class KVMGuest : public Guest {
				friend class KVMCpu;
				friend void ::MMIOThreadTrampoline(void *);

			public:
				KVMGuest(KVM& owner, engine::Engine& engine, jit::JIT& jit, const GuestConfiguration& config, int fd);
				virtual ~KVMGuest();

				bool init() override;
				bool load(loader::Loader& loader) override;

				CPU *create_cpu(const GuestCPUConfiguration& config) override;

				inline bool initialised() const { return _initialised; }

				bool stage2_init(uint64_t& stack);

				bool resolve_gpa(gpa_t gpa, void*& out_addr) const override;

				void do_guest_printf();

				virtual SharedMemory& shared_memory() const { return (SharedMemory&)_shared_memory; }

			private:
				class KVMSharedMemory : public SharedMemory
				{
					friend class KVMGuest;

				public:
					KVMSharedMemory();

					void *allocate(size_t size) override;
					void *reallocate(void *p, size_t size) override;
					void free(void *p) override;

				private:
					void set_arena(void *arena, size_t arena_size);

					void *_arena;
					size_t _arena_size;
				} _shared_memory;

				std::vector<KVMCpu *> kvm_cpus;

				bool _initialised;
				int fd, irq_fd;
				int next_cpu_id;
				int next_slot_idx;

				struct vm_mem_region {
					struct kvm_userspace_memory_region kvm;
					void *host_buffer;
				};

				PerGuestData *per_guest_data;

				std::list<vm_mem_region *> vm_mem_region_free;
				std::list<vm_mem_region *> vm_mem_region_used;

				struct gpm_desc {
					vm_mem_region *vmr;
					const GuestMemoryRegionConfiguration *cfg;
				};

				std::list<gpm_desc> gpm;

				struct dev_desc {
					devices::Device *dev;
					const GuestDeviceConfiguration *cfg;
				};

				std::list<dev_desc> devices;

				bool prepare_guest_irq();
				bool prepare_guest_memory();
				bool attach_guest_devices();
				devices::Device *lookup_device(uint64_t addr);

				bool install_bios();
				bool install_initial_pgt();
				bool install_gdt();
				bool install_tss();
				bool prepare_verification_memory();

				void *get_phys_buffer(uint64_t gpa);

				vm_mem_region *get_mem_slot();
				void put_mem_slot(vm_mem_region *region);

				vm_mem_region *alloc_guest_memory(uint64_t gpa, uint64_t size, uint32_t flags = 0, void *fixed_addr = NULL);
				void release_guest_memory(vm_mem_region *rgn);
				void release_all_guest_memory();

				typedef uint64_t pte_t;
				typedef pte_t *pm_t;
				typedef pte_t *pdp_t;
				typedef pte_t *pd_t;
				typedef pte_t *pt_t;

				inline uint64_t alloc_page()
				{
					uint64_t page = per_guest_data->next_phys_page;
					per_guest_data->next_phys_page += 0x1000;

					return page;
				}

				void map_page(uint64_t va, uint64_t pa, uint32_t flags);
				void map_huge_page(uint64_t va, uint64_t pa, uint32_t flags);

				inline int vmioctl(unsigned long int req) const {
					return vmioctl(req, (unsigned long int)0);
				}

				inline int vmioctl(unsigned long int req, unsigned long int arg) const {
					return ioctl(fd, req, arg);
				}

				inline int vmioctl(unsigned long int req, void *arg) const {
					return ioctl(fd, req, arg);
				}
			};
		}
	}
}

#endif	/* GUEST_H */

