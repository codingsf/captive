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
#include <unordered_map>

#include <sys/ioctl.h>

#include <define.h>
#include <shmem.h>

#include <hypervisor/guest.h>
#include <hypervisor/shared-memory.h>

#include <linux/kvm.h>

#define HOST_GPM_BASE			(uintptr_t)0x680100000000ULL
#define GUEST_GPM_VIRT_BASE		(uintptr_t)0x680100000000ULL
#define GUEST_GPM_PHYS_BASE		(uintptr_t)0x100000000ULL
#define GPM_SIZE				(size_t)0x100000000ULL

#define HOST_HEAP_BASE			(uintptr_t)0x680200000000ULL
#define GUEST_HEAP_VIRT_BASE	(uintptr_t)0x680200000000ULL
#define GUEST_HEAP_PHYS_BASE	(uintptr_t)0x200000000ULL
#define HEAP_SIZE				(size_t)0x100000000ULL

#define HOST_CODE_BASE			(uintptr_t)0x680000000000ULL
#define GUEST_CODE_VIRT_BASE	(uintptr_t)0xffffffff80000000ULL
#define GUEST_CODE_PHYS_BASE	(uintptr_t)0x0000000000000000ULL
#define CODE_SIZE				(size_t)0x40000000ULL

#define HOST_SYS_BASE			(uintptr_t)0x680040000000ULL
#define GUEST_SYS_VIRT_BASE		(uintptr_t)0x680040000000ULL
#define GUEST_SYS_PHYS_BASE		(uintptr_t)0x000040000000ULL
#define SYS_SIZE				(size_t)0x40000000ULL

#define GUEST_LAPIC_PHYS_BASE	(uintptr_t)0x0000fee00000ULL
#define GUEST_LAPIC_VIRT_BASE	(uintptr_t)0x67fffee00000ULL

#define SYS_GDT_OFFSET			0
#define SYS_IDT_OFFSET			0x1000
#define SYS_TSS_OFFSET			0x2000
#define SYS_PRINTF_OFFSET		0x3000
#define SYS_GUEST_DATA_OFFSET	0x4000
#define SYS_INIT_PGT_OFFSET		0x5000

#define HOST_SYS_GDT				(HOST_SYS_BASE + SYS_GDT_OFFSET)
#define HOST_SYS_IDT				(HOST_SYS_BASE + SYS_IDT_OFFSET)
#define HOST_SYS_TSS				(HOST_SYS_BASE + SYS_TSS_OFFSET)
#define HOST_SYS_PRINTF				(HOST_SYS_BASE + SYS_PRINTF_OFFSET)
#define HOST_SYS_GUEST_DATA			(HOST_SYS_BASE + SYS_GUEST_DATA_OFFSET)
#define HOST_SYS_INIT_PGT			(HOST_SYS_BASE + SYS_INIT_PGT_OFFSET)
#define HOST_SYS_KERNEL_STACK_TOP	(HOST_SYS_BASE + SYS_SIZE)

#define GUEST_SYS_GDT_PHYS			(GUEST_SYS_PHYS_BASE + SYS_GDT_OFFSET)
#define GUEST_SYS_GDT_VIRT			(GUEST_SYS_VIRT_BASE + SYS_GDT_OFFSET)
#define GUEST_SYS_INIT_PGT_PHYS		(GUEST_SYS_PHYS_BASE + SYS_INIT_PGT_OFFSET)
#define GUEST_SYS_GUEST_DATA_VIRT	(GUEST_SYS_VIRT_BASE + SYS_GUEST_DATA_OFFSET)
#define GUEST_SYS_PRINTF_VIRT		(GUEST_SYS_VIRT_BASE + SYS_PRINTF_OFFSET)
#define GUEST_SYS_KERNEL_STACK_TOP	(GUEST_SYS_VIRT_BASE + SYS_SIZE)

extern void MMIOThreadTrampoline(void *);

namespace captive {
	namespace devices {
		class Device;
	}

	namespace hypervisor {
		namespace kvm {
			class KVM;
			class KVMCpu;
			class IRQFD;
			
			class KVMGuest : public Guest {
				friend class KVMCpu;
				friend class IRQFD;
				friend void ::MMIOThreadTrampoline(void *);

			public:
				KVMGuest(KVM& owner, engine::Engine& engine, platform::Platform& pfm, int fd);
				virtual ~KVMGuest();

				bool init() override;
				bool load(loader::Loader& loader) override;

				inline bool initialised() const { return _initialised; }

				bool resolve_gpa(gpa_t gpa, void*& out_addr) const override;

				void do_guest_printf();
				
				bool run() override;
				void stop() override;
				
				void guest_entrypoint(gpa_t entrypoint) override;
				void debug_interrupt(int code) override;
				
			private:
				typedef bool (*event_callback_t)(int fd, bool is_input, void *data);

				struct event_loop_event
				{
					int fd;
					event_callback_t cb;
					void *data;
				};
				
				std::vector<KVMCpu *> kvm_cpus;
				static void core_thread_proc(KVMCpu *core);
				static void device_thread_proc(KVMGuest *guest);
				
				bool create_cpu(const GuestCPUConfiguration& config);

				bool _initialised;
				int fd;
				int next_cpu_id;
				int next_slot_idx;
				int epollfd, stopfd, intrfd;

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

				std::map<uint64_t, dev_desc> devices;

				bool prepare_event_loop();
				bool attach_event(int fd, event_callback_t cb, bool input, bool output, void *data);
				void cleanup_event_loop();
				bool prepare_guest_irq();
				bool prepare_guest_memory();
				bool attach_guest_devices();
				devices::Device *lookup_device(uint64_t addr, uint64_t& base_addr);

				bool install_gdt();
				bool install_tss();
				
				void dump_memory();
				
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

				uint64_t next_init_pgt_page;
				
				inline uint64_t alloc_init_pgt_page()
				{
					uint64_t next = next_init_pgt_page;
					next_init_pgt_page += 0x1000;
					
					return next;
				}
				
				void *sys_guest_phys_to_host_virt(uint64_t addr);

				void map_pages(uint64_t va, uint64_t pa, uint64_t size, uint32_t flags, bool use_huge_pages=true);
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
				
				static bool intr_callback(int fd, bool is_input, void *p);
			};
		}
	}
}

#endif	/* GUEST_H */

