#include <hypervisor/kvm/kvm.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/cpu.h>
#include <hypervisor/config.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/kvm.h>

#include "engine/engine.h"
#include "loader/loader.h"

using namespace captive::engine;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

#define BOOTSTRAP_BASE			0x0
#define BOOTSTRAP_SIZE			0x10000
#define GUEST_PHYS_MEMORY_BASE		0x100000000
#define GUEST_PHYS_MEMORY_MAX_SIZE	0x100000000
#define IDENTITY_BASE			0x50000000
#define TSS_BASE			0x50001000

#define DEFAULT_NR_SLOTS		32

extern char __bios_bin_start, __bios_bin_end;

KVMGuest::KVMGuest(KVM& owner, Engine& engine, const GuestConfiguration& config, int fd) : Guest(owner, engine, config), _initialised(false), fd(fd), next_cpu_id(0), next_slot_idx(0)
{

}

KVMGuest::~KVMGuest()
{
	if (initialised())
		release_all_guest_memory();

	DEBUG << "Closing KVM VM";
	close(fd);
}

bool KVMGuest::init()
{
	//	int rc;

	if (!Guest::init())
		return false;

	// Initialise memory region slot pool.
	for (int i = 0; i < DEFAULT_NR_SLOTS; i++) {
		vm_mem_region *region = new vm_mem_region();
		bzero(region, sizeof(*region));

		region->kvm.slot = i;

		vm_mem_region_free.push_back(region);
	}

	if (!prepare_guest_memory())
		return false;

	_initialised = true;
	return true;
}

bool KVMGuest::load(loader::Loader& loader)
{
	if (!initialised()) {
		ERROR << "KVM guest is not yet initialised";
		return false;
	}

	if (!loader.install((uint8_t *)gpm.front()->host_buffer)) {
		return false;
	}

	guest_entrypoint = loader.entrypoint();
	return true;
}

CPU* KVMGuest::create_cpu(const GuestCPUConfiguration& config)
{
	if (!initialised()) {
		ERROR << "KVM guest is not yet initialised";
		return NULL;
	}

	if (!config.validate()) {
		ERROR << "Invalid CPU configuration";
		return NULL;
	}

	DEBUG << "Creating KVM VCPU";
	int cpu_fd = ioctl(fd, KVM_CREATE_VCPU, next_cpu_id);
	if (cpu_fd < 0) {
		ERROR << "Failed to create KVM VCPU";
		return NULL;
	}

	KVMCpu *cpu = new KVMCpu(*this, config, next_cpu_id++, cpu_fd);
	kvm_cpus.push_back(cpu);

	return cpu;
}

bool KVMGuest::prepare_guest_memory()
{
	// Allocate the first 32MB for system usage.
	sys_mem_rgn = alloc_guest_memory(0x0, 0x2000000);
	if (!sys_mem_rgn) {
		ERROR << "Unable to allocate memory for the system";
		return false;
	}

	// Install the bios into the correct location.
	if (!install_bios()) {
		ERROR << "Unable to initialise bootstrap code";
		return false;
	}

	// Prepare initial guest page tables
	if (!install_initial_pgt()) {
		ERROR << "Unable to create initial page tables";
		return false;
	}

	// Install the engine code
	if (!engine().install(&((uint8_t *)sys_mem_rgn->host_buffer)[0x600000])) {
		ERROR << "Unable to install execution engine";
		return false;
	}

	DEBUG << "Installing guest memory regions";
	for (auto& region : config().memory_regions) {
		struct vm_mem_region *vm_region = alloc_guest_memory(GUEST_PHYS_MEMORY_BASE + region.base_address(), region.size());
		if (!vm_region) {
			release_all_guest_memory();
			ERROR << "Unable to allocate guest memory region";
			return false;
		}

		gpm.push_back(vm_region);
	}

	return true;
}

bool KVMGuest::install_bios()
{
	uint8_t *base = &((uint8_t *)sys_mem_rgn->host_buffer)[0xf0000];

	unsigned int bios_size = &__bios_bin_end - &__bios_bin_start;
	memcpy(base, &__bios_bin_start, bios_size);

	return true;
}

bool KVMGuest::install_initial_pgt()
{
	uint8_t *base = (uint8_t *)sys_mem_rgn->host_buffer;
	uint64_t *pg4 = (uint64_t *) (&base[0x1000]);
	uint64_t *pg3 = (uint64_t *) (&base[0x2000]);
	uint64_t *pg2 = (uint64_t *) (&base[0x3000]);
	uint64_t *pg1 = (uint64_t *) (&base[0x4000]);

	*pg4 = 0x2003;
	*pg3 = 0x3003;
	*pg2 = 0x4003;

	uint64_t addr = 3;
	for (int i = 0; i < 512; i++) {
		pg1[i] = addr;
		addr += 0x1000;
	}

	return true;
}

bool KVMGuest::stage2_init()
{
	uint8_t *base = (uint8_t *)sys_mem_rgn->host_buffer;

	// First, map the the emulated guest physical memory into virtual
	// memory
	uint64_t pt_addr = 0x4000;
	uint64_t phys_addr = GUEST_PHYS_MEMORY_BASE;

	uint64_t *pgd_base = (uint64_t *) (&base[0x3000]);
	for (uint32_t pgd_idx = 0; pgd_idx < 0x200; pgd_idx++) {
		pgd_base[pgd_idx] = pt_addr | 3;

		uint64_t *pt_base = (uint64_t *) (&base[pt_addr]);

		for (uint32_t pt_idx = 0; pt_idx < 0x200; pt_idx++) {
			pt_base[pt_idx] = phys_addr | 3;
			phys_addr += 0x1000;
		}

		pt_addr += 0x1000;
	}

	// Now, map the execution engine into virtual memory
	uint64_t *pdp_base = (uint64_t *) (&base[0x2000]);
	pdp_base[4] = pt_addr | 3;

	pgd_base = (uint64_t *) (&base[pt_addr]);
	phys_addr = 0x600000;
	pt_addr += 0x1000;
	for (uint32_t pgd_idx = 0; pgd_idx < 0x200; pgd_idx++) {
		pgd_base[pgd_idx] = pt_addr | 3;

		uint64_t *pt_base = (uint64_t *) (&base[pt_addr]);

		for (uint32_t pt_idx = 0; pt_idx < 0x200; pt_idx++) {
			pt_base[pt_idx] = phys_addr | 3;
			phys_addr += 0x1000;
		}

		pt_addr += 0x1000;
	}

	return true;
}


KVMGuest::vm_mem_region *KVMGuest::get_mem_slot()
{
	if (vm_mem_region_free.size() == 0)
		return NULL;

	vm_mem_region *region = vm_mem_region_free.front();
	vm_mem_region_free.pop_front();
	vm_mem_region_used.push_back(region);

	return region;
}

void KVMGuest::put_mem_slot(vm_mem_region* region)
{
	for (auto RI = vm_mem_region_used.begin(), RE = vm_mem_region_used.end(); RI != RE; ++RI) {
		if (*RI == region) {
			vm_mem_region_used.erase(RI);
			vm_mem_region_free.push_back(region);
			break;
		}
	}
}

KVMGuest::vm_mem_region *KVMGuest::alloc_guest_memory(uint64_t gpa, uint64_t size)
{
	// Try to obtain a free memory region slot.
	vm_mem_region *rgn = get_mem_slot();
	if (rgn == NULL)
		return NULL;

	// Fill in the KVM memory structure.
	rgn->kvm.flags = 0;
	rgn->kvm.guest_phys_addr = gpa;
	rgn->kvm.memory_size = size;

	// Allocate a userspace buffer for the region.
	rgn->host_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS, -1, 0);
	if (rgn->host_buffer == MAP_FAILED) {
		put_mem_slot(rgn);

		ERROR << "Unable to allocate memory";
		return NULL;
	}

	// Store the buffer address in the KVM memory structure.
	rgn->kvm.userspace_addr = (uint64_t) rgn->host_buffer;

	// Install the memory region into the guest.
	int rc = ioctl(fd, KVM_SET_USER_MEMORY_REGION, &rgn->kvm);
	if (rc) {
		munmap(rgn->host_buffer, rgn->kvm.memory_size);
		put_mem_slot(rgn);

		ERROR << "Unable to install memory";
		return NULL;
	}

	DEBUG << "Allocated guest memory, gpa=" << std::hex << rgn->kvm.guest_phys_addr << ", size=" << std::hex << rgn->kvm.memory_size << ", hva=" << std::hex << rgn->host_buffer;
	return rgn;
}

void KVMGuest::release_guest_memory(vm_mem_region *region)
{
	struct kvm_userspace_memory_region kvm;

	// Copy the KVM memory structure, so that we can tinker with it to
	// tell KVM to release it.
	kvm = region->kvm;

	kvm.memory_size = 0;

	// Remove the memory region from the guest.
	ioctl(fd, KVM_SET_USER_MEMORY_REGION, &kvm);

	// Release the associated buffer.
	munmap(region->host_buffer, region->kvm.memory_size);

	// Return the memory slot to the free pool.
	put_mem_slot(region);
}

void KVMGuest::release_all_guest_memory()
{
	// Copy the list, to avoid destroying something we're iterating over.
	std::list<vm_mem_region *> used_regions = vm_mem_region_used;

	for (auto region : used_regions) {
		release_guest_memory(region);
	}

	gpm.clear();
}
