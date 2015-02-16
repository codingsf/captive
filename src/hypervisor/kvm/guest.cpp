#include <hypervisor/kvm/kvm.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/cpu.h>
#include <hypervisor/config.h>
#include <loader/loader.h>
#include <engine/engine.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <linux/kvm.h>

#include "devices/device.h"

using namespace captive::engine;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

#define GUEST_PHYS_MEMORY_BASE		0x100000000ULL
#define GUEST_PHYS_MEMORY_VIRT_BASE	0ULL
#define GUEST_PHYS_MEMORY_MAX_SIZE	0x100000000ULL

#define SYSTEM_MEMORY_PHYS_BASE		0ULL
#define SYSTEM_MEMORY_PHYS_SIZE		0x40000000ULL

#define ENGINE_PHYS_BASE		(SYSTEM_MEMORY_PHYS_BASE + 0x10000000ULL)
#define ENGINE_VIRT_BASE		0x100000000ULL
#define ENGINE_SIZE			(SYSTEM_MEMORY_PHYS_SIZE - ENGINE_PHYS_BASE)

#define IDMAP_PHYS_ADDR			(SYSTEM_MEMORY_PHYS_BASE + SYSTEM_MEMORY_PHYS_SIZE)
#define IDMAP_PHYS_SIZE			(1 * 0x1000)

#define TSS_PHYS_ADDR			(IDMAP_PHYS_ADDR + IDMAP_PHYS_SIZE)
#define TSS_PHYS_SIZE			(3 * 0x1000)

#define DATA_PHYS_BASE			SYSTEM_MEMORY_PHYS_BASE
#define DATA_VIRT_BASE			0x200000000ULL
#define DATA_SIZE			(ENGINE_PHYS_BASE - SYSTEM_MEMORY_PHYS_BASE)

#define BIOS_PHYS_BASE			0xf0000ULL

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

	if (!attach_guest_devices())
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

	if (!loader.install((uint8_t *)gpm.front().vmr->host_buffer)) {
		return false;
	}

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

bool KVMGuest::attach_guest_devices()
{
	for (const auto& device : config().devices) {
		DEBUG << "Attaching device @ " << std::hex << device.base_address();

		device.device().attach(*this);

		dev_desc desc;
		desc.cfg = &device;
		desc.dev = &device.device();

		devices.push_back(desc);
	}

	return true;
}

captive::devices::Device *KVMGuest::lookup_device(uint64_t addr)
{
	for (const auto& desc : devices) {
		if (addr >= desc.cfg->base_address() && addr <= desc.cfg->base_address() + desc.cfg->size()) {
			return desc.dev;
		}
	}

	return NULL;
}

bool KVMGuest::prepare_guest_memory()
{
	/*unsigned int rc;

	rc = vmioctl(KVM_CAP_CHECK_EXTENSION_VM, KVM_CAP_SET_IDENTITY_MAP_ADDR);
	if (rc) {
		if (!alloc_guest_memory(IDMAP_PHYS_ADDR, IDMAP_PHYS_SIZE)) {
			ERROR << "Unable to allocate storage for IDMAP";
			return false;
		}

		DEBUG << "Setting IDMAP to " << std::hex << IDMAP_PHYS_ADDR;
		unsigned long idmap_addr = IDMAP_PHYS_ADDR;
		rc = vmioctl(KVM_SET_IDENTITY_MAP_ADDR, &idmap_addr);
		if (rc) {
			ERROR << "Unable to set IDMAP address";
			return false;
		}
	}

	rc = vmioctl(KVM_CAP_CHECK_EXTENSION_VM, KVM_CAP_SET_TSS_ADDR);
	if (rc) {
		if (!alloc_guest_memory(TSS_PHYS_ADDR, TSS_PHYS_SIZE)) {
			ERROR << "Unable to allocate storage for TSS";
			return false;
		}

		DEBUG << "Setting TSS to " << std::hex << TSS_PHYS_ADDR;
		unsigned long tss_addr = TSS_PHYS_ADDR;
		rc = vmioctl(KVM_SET_TSS_ADDR, tss_addr);
		if (rc) {
			ERROR << "Unable to set TSS address";
			return false;
		}
	}*/

	// Allocate the first 1GB for system usage.
	sys_mem_rgn = alloc_guest_memory(SYSTEM_MEMORY_PHYS_BASE, SYSTEM_MEMORY_PHYS_SIZE);
	if (!sys_mem_rgn) {
		ERROR << "Unable to allocate memory for the system";
		return false;
	}

	// Hack in the GDT
	uint64_t *gdt = (uint64_t *)&((uint8_t *)sys_mem_rgn->host_buffer)[0x10];

	*gdt++ = 0;
	*gdt++ = 0x0020980000000000;
	*gdt++ = 0x0000900000000000;

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
	if (!engine().install(&((uint8_t *)sys_mem_rgn->host_buffer)[ENGINE_PHYS_BASE])) {
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

		gpm_desc desc;
		desc.cfg = &region;
		desc.vmr = vm_region;

		gpm.push_back(desc);
	}

	return true;
}

bool KVMGuest::install_bios()
{
	uint8_t *base = &((uint8_t *)sys_mem_rgn->host_buffer)[BIOS_PHYS_BASE];

	unsigned int bios_size = &__bios_bin_end - &__bios_bin_start;
	memcpy(base, &__bios_bin_start, bios_size);

	return true;
}

#define PT_PRESENT	(1ULL << 0)
#define PT_WRITABLE	(1ULL << 1)
#define PT_NO_EXECUTE	(1ULL << 63)

bool KVMGuest::install_initial_pgt()
{
	next_page = 0x2000;

	for (uint64_t va = 0, pa = 0; va < 0x200000; va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT | PT_WRITABLE);
	}

	return true;
}

bool KVMGuest::stage2_init()
{
	// Map the ENGINE into memory
	for (uint64_t va = ENGINE_VIRT_BASE, pa = ENGINE_PHYS_BASE; va < (ENGINE_VIRT_BASE + ENGINE_SIZE); va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT | PT_WRITABLE);
	}

	// Map the DATA area into memory
	for (uint64_t va = DATA_VIRT_BASE, pa = DATA_PHYS_BASE; va < (DATA_VIRT_BASE + DATA_SIZE); va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT | PT_WRITABLE);
	}

	// Map ALL guest physical memory, but don't mark it as present.
	for (uint64_t va = GUEST_PHYS_MEMORY_VIRT_BASE, pa = GUEST_PHYS_MEMORY_BASE; va < (GUEST_PHYS_MEMORY_VIRT_BASE + GUEST_PHYS_MEMORY_MAX_SIZE); va += 0x1000, pa += 0x1000) {
		map_page(va, pa, 0);
	}

	// Remap only the available physical memory.
	for (const auto& pmr : gpm) {
		for (uint64_t va = pmr.cfg->base_address(), pa = pmr.vmr->kvm.guest_phys_addr; va < pmr.cfg->base_address() + pmr.cfg->size(); va += 0x1000, pa += 0x1000) {
			map_page(va, pa, PT_PRESENT | PT_WRITABLE);
		}
	}

	// Remap devices
	for (const auto& dev : devices) {
		uint64_t va = dev.cfg->base_address();
		uint64_t pa = GUEST_PHYS_MEMORY_BASE + va;

		for (int i = 0; i < dev.cfg->size() / 0x1000; i++) {
			map_page(va, pa, PT_PRESENT | PT_WRITABLE);
			va += 0x1000;
			pa += 0x1000;
		}
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

KVMGuest::vm_mem_region *KVMGuest::alloc_guest_memory(uint64_t gpa, uint64_t size, uint32_t flags)
{
	// Try to obtain a free memory region slot.
	vm_mem_region *rgn = get_mem_slot();
	if (rgn == NULL)
		return NULL;

	// Fill in the KVM memory structure.
	rgn->kvm.flags = flags;
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

#define ADDR_MASK (uint64_t)~((uint64_t)(0xfff))
#define FLAGS_MASK (uint64_t)((uint64_t)(0xfff))

#define BITS(val, start, end) ((val >> start) & (((1 << (end - start + 1)) - 1)))

void KVMGuest::map_page(uint64_t va, uint64_t pa, uint32_t flags)
{
	uint8_t *base = (uint8_t *)sys_mem_rgn->host_buffer;

	uint16_t pm_idx = BITS(va, 39, 47);
	uint16_t pdp_idx = BITS(va, 30, 38);
	uint16_t pd_idx = BITS(va, 21, 29);
	uint16_t pt_idx = BITS(va, 12, 20);

	pm_t pm = (pm_t)&base[0x1000];
	if (pm[pm_idx] == 0) {
		uint64_t new_page = alloc_page();
		bzero(&base[new_page], 0x1000);

		pm[pm_idx] = new_page | PT_PRESENT | PT_WRITABLE;
	}

	pdp_t pdp = (pdp_t)&base[pm[pm_idx] & ADDR_MASK];
	if (pdp[pdp_idx] == 0) {
		uint64_t new_page = alloc_page();
		bzero(&base[new_page], 0x1000);

		pdp[pdp_idx] = new_page | PT_PRESENT | PT_WRITABLE;
	}

	pd_t pd = (pd_t)&base[pdp[pdp_idx] & ADDR_MASK];
	if (pd[pd_idx] == 0) {
		uint64_t new_page = alloc_page();
		bzero(&base[new_page], 0x1000);

		pd[pd_idx] = new_page | PT_PRESENT | PT_WRITABLE;
	}

	pt_t pt = (pt_t)&base[pd[pd_idx] & ADDR_MASK];
	pt[pt_idx] = (pa & ADDR_MASK) | (flags & FLAGS_MASK);

	/*DEBUG << "Map Page VA=" << std::hex << va
		<< ", PA=" << pa
		<< ", PM[1000]=" << (uint32_t)pm_idx
		<< ", PDP[" << pm[pm_idx] << "]=" << (uint32_t)pdp_idx
		<< ", PD[" << pdp[pdp_idx] << "]=" << (uint32_t)pd_idx
		<< ", PT[" << pd[pd_idx] << "]=" << (uint32_t)pt_idx;*/
}
