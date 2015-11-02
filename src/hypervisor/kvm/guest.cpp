#include <hypervisor/kvm/kvm.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/cpu.h>
#include <hypervisor/config.h>
#include <platform/platform.h>
#include <loader/loader.h>
#include <engine/engine.h>
#include <devices/device.h>
#include <shmem.h>

#include <thread>
#include <pthread.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <linux/kvm.h>

USE_CONTEXT(Guest);

using namespace captive::engine;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

#define VERBOSE_ENABLED			false

#define DEFAULT_NR_SLOTS		32

#define GPM_BASE_GPA			0x000000000ULL
#define HEAP_BASE_GPA			0x100000000ULL
#define EE_BASE_GPA				0x200000000ULL

#define GPM_BASE_HVA			0x7f1000000000ULL
#define HEAP_BASE_HVA			0x7f1100000000ULL
#define EE_BASE_HVA				0x7f1200000000ULL

#define SEGMENT_SIZE			0x100000000ULL

KVMGuest::KVMGuest(KVM& owner, Engine& engine, platform::Platform& pfm, int fd) 
	: Guest(owner, engine, pfm),
		_initialised(false),
		fd(fd),
		next_cpu_id(0),
		next_slot_idx(0)
{

}

KVMGuest::~KVMGuest()
{
	if (initialised())
		release_all_guest_memory();

	DEBUG << CONTEXT(Guest) << "Closing KVM VM";
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

	if (!prepare_guest_irq())
		return false;

	if (!prepare_guest_memory())
		return false;

	if (!attach_guest_devices())
		return false;
	
	engine().install((uint8_t *)EE_BASE_HVA);
	
	for (auto core : platform().config().cores) {
		if (!create_cpu(core)) {
			return false;
		}
	}

	_initialised = true;
	return true;
}

bool KVMGuest::load(loader::Loader& loader)
{
	if (!initialised()) {
		ERROR << "KVM guest is not yet initialised";
		return false;
	}

	if (!loader.install((uint8_t *)get_phys_buffer(0))) {
		ERROR << CONTEXT(Guest) << "Loader failed to install";
		return false;
	}

	return true;
}

bool KVMGuest::create_cpu(const GuestCPUConfiguration& config)
{
	if (!config.validate()) {
		ERROR << "Invalid CPU configuration";
		return NULL;
	}

	DEBUG << CONTEXT(Guest) << "Creating KVM VCPU";
	int cpu_fd = ioctl(fd, KVM_CREATE_VCPU, next_cpu_id);
	if (cpu_fd < 0) {
		ERROR << "Failed to create KVM VCPU";
		return NULL;
	}

	// Locate the storage location for the Per-CPU data, and initialise the structure.
	PerCPUData *per_cpu_data = (PerCPUData *)get_phys_buffer(0);	// TODO: FIXME: ADDRESS
	per_cpu_data->async_action = 0;
	per_cpu_data->execution_mode = 0;
	per_cpu_data->guest_data = per_guest_data;
	per_cpu_data->insns_executed = 0;
	per_cpu_data->interrupts_taken = 0;
	per_cpu_data->isr = 0;
	
	per_cpu_data->verbose_enabled = VERBOSE_ENABLED;

	KVMCpu *cpu = new KVMCpu(*this, config, next_cpu_id, cpu_fd, irq_fd, per_cpu_data);
	if (!cpu->init()) {
		delete cpu;
		return false;
	}
	
	kvm_cpus.push_back(cpu);

	next_cpu_id++;
	return true;
}

bool KVMGuest::run()
{
	std::list<std::thread *> core_threads;
	
	for (auto core : kvm_cpus) {
		auto core_thread = new std::thread(core_thread_proc, core);
		core_threads.push_back(core_thread);
	}
	
	for (auto thread : core_threads) {
		if (thread->joinable()) thread->join();
	}
	
	return true;
}

void KVMGuest::core_thread_proc(KVMCpu *core)
{
	core->run();
}

bool KVMGuest::attach_guest_devices()
{
	for (const auto& device : platform().config().devices) {
		DEBUG << CONTEXT(Guest) << "Attaching device " << device.device().name() << " @ " << std::hex << device.base_address();

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
		if (addr >= desc.cfg->base_address() && addr < desc.cfg->base_address() + desc.dev->size()) {
			return desc.dev;
		}
	}

	return NULL;
}

bool KVMGuest::prepare_guest_irq()
{
	DEBUG << CONTEXT(Guest) << "Creating IRQ chip";
	if (vmioctl(KVM_CREATE_IRQCHIP)) {
		ERROR << "Unable to create IRQCHIP";
		return false;
	}

	struct kvm_irqchip irqchip;
	bzero(&irqchip, sizeof(irqchip));
	irqchip.chip_id = 2;

	if (vmioctl(KVM_GET_IRQCHIP, &irqchip)) {
		ERROR << "Unable to retrieve IRQCHIP";
		return false;
	}

	// GSI 16 will be our externally signalled interrupt - route it to
	// vector 0x30, and unmask it.  Also, set it to be level triggered
	irqchip.chip.ioapic.redirtbl[16].fields.vector = 0x30;
	irqchip.chip.ioapic.redirtbl[16].fields.trig_mode = 1;
	irqchip.chip.ioapic.redirtbl[16].fields.mask = 0;

	DEBUG << CONTEXT(Guest) << "Configuring IRQ chip";
	if (vmioctl(KVM_SET_IRQCHIP, &irqchip)) {
		ERROR << "Unable to configure IRQCHIP";
		return false;
	}

	DEBUG << CONTEXT(Guest) << "Creating IRQ fd";
	irq_fd = eventfd(0, O_NONBLOCK | O_CLOEXEC);
	if (irq_fd < 0) {
		ERROR << "Unable to create IRQ fd";
		return false;
	}

	struct kvm_irqfd irqfd;
	bzero(&irqfd, sizeof(irqfd));
	irqfd.fd = irq_fd;
	irqfd.gsi = 16;

	if (vmioctl(KVM_IRQFD, &irqfd)) {
		ERROR << "Unable to install IRQ fd";
		return false;
	}

	return true;
}

bool KVMGuest::prepare_guest_memory()
{
	uint64_t gpm_host_base = GPM_BASE_HVA;
	for (auto rgn : platform().config().memory_regions) {
		if (!alloc_guest_memory(rgn.base_address(), rgn.size(), 0, (void *)(gpm_host_base | (rgn.base_address() & 0xffffffffULL)))) {
			ERROR << "Unable to allocate GPM memory";
			return false;
		}
	}
	
	if (!alloc_guest_memory(HEAP_BASE_GPA, SEGMENT_SIZE, 0, (void *)HEAP_BASE_HVA)) {
		ERROR << "Unable to allocate HEAP memory";
		return false;
	}
	
	if (!alloc_guest_memory(EE_BASE_GPA,   SEGMENT_SIZE, 0, (void *)EE_BASE_HVA)) {
		ERROR << "Unable to allocate EE memory";
		return false;
	}
	
	if (!install_gdt()) {
		ERROR << CONTEXT(Guest) << "Unable to install GDT";
		return false;
	}

	if (!install_tss()) {
		ERROR << CONTEXT(Guest) << "Unable to install TSS";
		return false;
	}

	return true;
}

bool KVMGuest::install_gdt()
{
	// Hack in the GDT
	uint64_t *gdt = (uint64_t *)get_phys_buffer(0x100000000);

	*gdt++ = 0;							// NULL
	*gdt++ = 0x0020980000000000;		// KERNEL CS
	*gdt++ = 0x0000920000000000;		// KERNEL DS
	*gdt++ = 0x0020f80000000000;		// USER CS
	*gdt++ = 0x0000f20000000000;		// USER DS
	*gdt++ = 0x00408900130000d0;		// TSS
	*gdt = 2;

	return true;
}

bool KVMGuest::install_tss()
{
	// Hack in the TSS
	uint64_t *tss = (uint64_t *)get_phys_buffer(0x1304);

	// TODO: FIXME
	//tss[0] = SYSTEM_DATA_VIRT_BASE + SYSTEM_DATA_SIZE;

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

KVMGuest::vm_mem_region *KVMGuest::alloc_guest_memory(uint64_t gpa, uint64_t size, uint32_t flags, void *fixed_addr)
{
	// Try to obtain a free memory region slot.
	vm_mem_region *rgn = get_mem_slot();
	if (rgn == NULL) {
		ERROR << "Unable to acquire memory slot";
		return NULL;
	}

	// Fill in the KVM memory structure.
	rgn->kvm.flags = flags;
	rgn->kvm.guest_phys_addr = gpa;
	rgn->kvm.memory_size = size;

	// Allocate a userspace buffer for the region.
	int mmap_flags = MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS;

	if (fixed_addr) {
		mmap_flags |= MAP_FIXED;
	}

	int mmap_prot = PROT_READ | PROT_WRITE;

	rgn->host_buffer = mmap(fixed_addr, size, mmap_prot, mmap_flags, -1, 0);
	if (rgn->host_buffer == MAP_FAILED) {
		put_mem_slot(rgn);

		ERROR << "Unable to allocate memory";
		return NULL;
	}

	// Store the buffer address in the KVM memory structure.
	rgn->kvm.userspace_addr = (uint64_t) rgn->host_buffer;

	DEBUG << CONTEXT(Guest) << "Installing memory region into guest";

	// Install the memory region into the guest.
	int rc = ioctl(fd, KVM_SET_USER_MEMORY_REGION, &rgn->kvm);
	if (rc) {
		munmap(rgn->host_buffer, rgn->kvm.memory_size);
		put_mem_slot(rgn);

		ERROR << "Unable to install memory";
		return NULL;
	}

	DEBUG << CONTEXT(Guest) << "Allocated guest memory, gpa=" << std::hex << rgn->kvm.guest_phys_addr << ", size=" << std::hex << rgn->kvm.memory_size << ", hva=" << std::hex << rgn->host_buffer;
	return rgn;
}

void* KVMGuest::get_phys_buffer(uint64_t gpa) const
{
	if (gpa < 0x300000000ULL) {
		return (void *)((0x7f1000000000ULL) | (gpa & 0x3ffffffff));
	} else {
		return NULL;
	}
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

bool KVMGuest::resolve_gpa(gpa_t gpa, void*& out_addr) const
{
	void *buffer = get_phys_buffer((uint64_t)gpa);
	if (buffer) {
		out_addr = buffer;
		return true;
	} else {
		return false;
	}
}

void KVMGuest::do_guest_printf()
{
	fprintf(stderr, "%s", "X"); // TODO: (const char *)per_guest_data->printf_buffer.base_address);
}
