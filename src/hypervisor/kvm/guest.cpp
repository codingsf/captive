#include <hypervisor/kvm/kvm.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/cpu.h>
#include <hypervisor/config.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/kvm.h>

#include "engine/engine.h"

using namespace captive::engine;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

#define BOOTSTRAP_BASE			0x0
#define BOOTSTRAP_SIZE			0x10000
#define GUEST_PHYS_MEMORY_BASE		0x100000000
#define GUEST_PHYS_MEMORY_MAX_SIZE	0x100000000
#define IDENTITY_BASE			0x50000000
#define TSS_BASE			0x50001000

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

	/*DEBUG << "Setting identity map to " << std::hex << IDENTITY_BASE;
	uint64_t idb = IDENTITY_BASE;
	rc = ioctl(fd, KVM_SET_IDENTITY_MAP_ADDR, &idb);
	if (rc) {
		ERROR << "Unable to set identity map address to " << std::hex << IDENTITY_BASE;
		return false;
	}

	DEBUG << "Setting TSS to " << std::hex << TSS_BASE;
	rc = ioctl(fd, KVM_SET_TSS_ADDR, TSS_BASE);
	if (rc) {
		ERROR << "Unable to set TSS address to " << std::hex << TSS_BASE;
		return false;
	}

	DEBUG << "Creating CPU IRQ chip";
	rc = ioctl(fd, KVM_CREATE_IRQCHIP, 0);
	if (rc) {
		release_guest_memory();
		ERROR << "Unable to create IRQ chip";
		return false;
	}*/

	if (!prepare_guest_memory())
		return false;

	_initialised = true;
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
	int rc;

	const struct vm_mem_region *bootstrap = alloc_guest_memory(0xffff0000, 0x10000);
	if (!bootstrap) {
		ERROR << "Unable to allocate memory for the bootstrap";
		return false;
	}

	if (!prepare_bootstrap((uint8_t *)bootstrap->host_buffer)) {
		ERROR << "Unable to prepare bootstrap";
		return false;
	}

	DEBUG << "Installing guest memory regions";
	for (auto& region : config().memory_regions) {
		const struct vm_mem_region *vm_region = alloc_guest_memory(GUEST_PHYS_MEMORY_BASE + region.base_address(), region.size());
		if (!vm_region) {
			release_all_guest_memory();
			ERROR << "Unable to allocate guest memory region";
			return false;
		}
	}

	return true;
}

bool KVMGuest::prepare_bootstrap(uint8_t* base)
{
	base[0xfff0] = 0xb8;
	base[0xfff1] = 0xff;
	base[0xfff3] = 0xcc;

	return true;
}

const KVMGuest::vm_mem_region *KVMGuest::alloc_guest_memory(uint64_t gpa, uint64_t size)
{
	// TODO: Check that there is an available slot - maintain a slot index queue?
	// or maybe just a vm_mem_region free/used pool with pre-allocated slot
	// indicies.

	struct vm_mem_region *rgn = new struct vm_mem_region();

	// Fill in the KVM memory structure.
	rgn->kvm.flags = 0;
	rgn->kvm.guest_phys_addr = gpa;
	rgn->kvm.memory_size = size;
	rgn->kvm.slot = next_slot_idx++;

	// Allocate a userspace buffer for the region.
	rgn->host_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS, -1, 0);
	if (rgn->host_buffer == MAP_FAILED) {
		delete rgn;

		ERROR << "Unable to allocate memory";
		return NULL;
	}

	// Store the buffer address in the KVM memory structure.
	rgn->kvm.userspace_addr = (uint64_t)rgn->host_buffer;

	// Install the memory region into the guest.
	int rc = ioctl(fd, KVM_SET_USER_MEMORY_REGION, &rgn->kvm);
	if (rc) {
		munmap(rgn->host_buffer, rgn->kvm.memory_size);
		delete rgn;

		ERROR << "Unable to install memory";
		return NULL;
	}

	DEBUG << "Allocated guest memory, gpa=" << std::hex << rgn->kvm.guest_phys_addr << ", size=" << std::hex << rgn->kvm.memory_size;
	vm_mem_regions.push_back(rgn);
	return rgn;
}

void KVMGuest::release_guest_memory(const vm_mem_region *region)
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
	delete region;
}

void KVMGuest::release_all_guest_memory()
{
	for (auto region : vm_mem_regions) {
		release_guest_memory(region);
	}
}