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

#define PAGE_SIZE			4096
#define GUEST_PHYS_MEMORY_BASE		0x0
#define GUEST_PHYS_MEMORY_MAX_SIZE	0x100000000
#define HV_MEMORY_BASE			0x100000000
#define BOOTLOADER_MEMORY_BASE		0
//HV_MEMORY_BASE

#define IDENTITY_SIZE			PAGE_SIZE
#define TSS_SIZE			(PAGE_SIZE * 3)
#define IDENTITY_BASE			(GUEST_PHYS_MEMORY_MAX_SIZE - TSS_SIZE - IDENTITY_SIZE)
#define TSS_BASE			(IDENTITY_BASE + IDENTITY_SIZE)

KVMGuest::KVMGuest(KVM& owner, Engine& engine, const GuestConfiguration& config, int fd) : Guest(owner, engine, config), _initialised(false), fd(fd), next_cpu_id(0)
{

}

KVMGuest::~KVMGuest()
{
	if (initialised())
		release_guest_memory();

	DEBUG << "Closing KVM VM";
	close(fd);
}

bool KVMGuest::init()
{
	int rc;

	if (!Guest::init())
		return false;

	DEBUG << "Setting identity map to " << std::hex << IDENTITY_BASE;
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
	}

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
	int slot_index = 0;

	struct kvm_userspace_memory_region bootloader_rgn;
	bootloader_rgn.slot = slot_index++;
	bootloader_rgn.flags = 0; //KVM_MEM_READONLY;
	bootloader_rgn.guest_phys_addr = BOOTLOADER_MEMORY_BASE;
	bootloader_rgn.memory_size = engine().get_bootloader_size();
	bootloader_rgn.userspace_addr = (uint64_t)engine().get_bootloader_addr();

	DEBUG << "Installing bootloader " <<
	"gpa=" << std::hex << bootloader_rgn.guest_phys_addr <<
	", size=" << std::hex << bootloader_rgn.memory_size <<
	", hva=" << std::hex << bootloader_rgn.userspace_addr;

	rc = ioctl(fd, KVM_SET_USER_MEMORY_REGION, &bootloader_rgn);
	if (rc) {
		ERROR << "Unable to install bootloader";
		return false;
	}

	return true;

	for (auto& region : config().memory_regions) {
		struct vm_mem_region vm_region;

		vm_region.buffer_size = region.size();
		vm_region.buffer = (uint64_t)mmap(NULL, vm_region.buffer_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS, -1, 0);

		if ((void *)vm_region.buffer == MAP_FAILED) {
			ERROR << "Unable to allocate storage for memory region";
			return false;
		}

		vm_region.kvm.slot = slot_index++;
		vm_region.kvm.flags = KVM_MEM_LOG_DIRTY_PAGES;
		vm_region.kvm.guest_phys_addr = GUEST_PHYS_MEMORY_BASE + region.base_address();
		vm_region.kvm.memory_size = vm_region.buffer_size;
		vm_region.kvm.userspace_addr = vm_region.buffer;

		DEBUG << "Installing guest memory region " <<
			"gpa=" << std::hex << vm_region.kvm.guest_phys_addr <<
			", size=" << std::hex << vm_region.kvm.memory_size <<
			", hva=" << std::hex << vm_region.kvm.userspace_addr;

		// Instruct the VM to create a region for guest physical memory
		rc = ioctl(fd, KVM_SET_USER_MEMORY_REGION, &vm_region.kvm);
		if (rc) {
			ERROR << "Unable to initialise VM memory region";
			munmap((void *)vm_region.buffer, vm_region.buffer_size);
			return false;
		}

		vm_mem_regions.push_back(vm_region);
	}

	return true;
}

void KVMGuest::release_guest_memory()
{
	for (auto vm_region : vm_mem_regions) {
		munmap((void *)vm_region.buffer, vm_region.buffer_size);
	}

	vm_mem_regions.clear();
}
