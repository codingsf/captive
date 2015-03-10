#include <hypervisor/kvm/kvm.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/cpu.h>
#include <hypervisor/config.h>
#include <jit/jit.h>
#include <loader/loader.h>
#include <engine/engine.h>
#include <devices/device.h>
#include <shmem.h>
#include <verify.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <linux/kvm.h>

USE_CONTEXT(Guest);

using namespace captive::engine;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

#define GUEST_PHYS_MEMORY_BASE			0x100000000ULL
#define GUEST_PHYS_MEMORY_VIRT_BASE		0ULL
#define GUEST_PHYS_MEMORY_MAX_SIZE		0x100000000ULL
#define GUEST_PHYS_MEMORY_COPY_VIRT_BASE	(GUEST_PHYS_MEMORY_VIRT_BASE + GUEST_PHYS_MEMORY_MAX_SIZE)

#define SYSTEM_MEMORY_PHYS_BASE		0ULL
#define SYSTEM_MEMORY_PHYS_SIZE		0x40000000ULL

#define ENGINE_PHYS_BASE		(SYSTEM_MEMORY_PHYS_BASE + 0x10000000ULL)
#define ENGINE_VIRT_BASE		0xFFFFFFFF80000000ULL
#define ENGINE_SIZE			(SYSTEM_MEMORY_PHYS_SIZE - ENGINE_PHYS_BASE)

#define DATA_PHYS_BASE			SYSTEM_MEMORY_PHYS_BASE
#define DATA_VIRT_BASE			0x200000000ULL
#define DATA_SIZE			(ENGINE_PHYS_BASE - SYSTEM_MEMORY_PHYS_BASE)

#define SHMEM_PHYS_BASE			(SYSTEM_MEMORY_PHYS_BASE + SYSTEM_MEMORY_PHYS_SIZE)
#define SHMEM_VIRT_BASE			(DATA_VIRT_BASE + DATA_SIZE)
#define SHMEM_PHYS_SIZE			0x10000000ULL

#define JIT_PHYS_BASE			(SHMEM_PHYS_BASE + SHMEM_PHYS_SIZE)
#define JIT_VIRT_BASE			(SHMEM_VIRT_BASE + SHMEM_PHYS_SIZE)
#define JIT_SIZE			0x10000000ULL

#define PER_CPU_PHYS_BASE		(JIT_PHYS_BASE + JIT_SIZE)
#define PER_CPU_PHYS_SIZE		0x10000

#define BIOS_PHYS_BASE			0xf0000ULL

struct {
	std::string name;
	uint64_t phys_base;
	uint64_t virt_base;
	uint64_t size;
} guest_memory_regions[] = {
	{ .name = "system-data", .phys_base = 0,              .virt_base = 0x200000000ULL,        .size = 0x010000000ULL },
	{ .name = "engine",      .phys_base = 0x010000000ULL, .virt_base = 0xFFFFFFFF80000000ULL, .size = 0x030000000ULL },
	{ .name = "shared-mem",  .phys_base = 0x040000000ULL, .virt_base = 0x210000000ULL,        .size = 0x010000000ULL },
	{ .name = "jit-mem",     .phys_base = 0x050000000ULL, .virt_base = 0x220000000ULL,        .size = 0x010000000ULL },
	{ .name = "per-cpu-mem", .phys_base = 0x060000000ULL, .virt_base = 0x230000000ULL,        .size = 0x000010000ULL },
	{ .name = "gpm",         .phys_base = 0x100000000ULL, .virt_base = 0,                     .size = 0x100000000ULL },
	{ .name = "gpm-copy",    .phys_base = 0x100000000ULL, .virt_base = 0x100000000ULL,        .size = 0x100000000ULL },
};

#define DEFAULT_NR_SLOTS		32

extern char __bios_bin_start, __bios_bin_end;

KVMGuest::KVMGuest(KVM& owner, Engine& engine, jit::JIT& jit, const GuestConfiguration& config, int fd) : Guest(owner, engine, jit, config), _initialised(false), fd(fd), next_cpu_id(0), next_slot_idx(0)
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

	if (!prepare_guest_irq())
		return false;

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

	// Allocate storage for the per-cpu data structure
	vm_mem_region *per_cpu = alloc_guest_memory(PER_CPU_PHYS_BASE + (0x1000 * next_cpu_id), 0x1000, 0);

	KVMCpu *cpu = new KVMCpu(*this, config, next_cpu_id++, cpu_fd, per_cpu->host_buffer);
	kvm_cpus.push_back(cpu);

	return cpu;
}

bool KVMGuest::attach_guest_devices()
{
	for (const auto& device : config().devices) {
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
	DEBUG << "Creating IRQ chip";
	if (vmioctl(KVM_CREATE_IRQCHIP)) {
		ERROR << "Unable to create IRQCHIP";
		return false;
	}

	struct kvm_irqchip irqchip;
	bzero(&irqchip, sizeof(irqchip));

	irqchip.chip_id = 2;
	irqchip.chip.ioapic.base_address = 0x1000;
	irqchip.chip.ioapic.id = 0;
	irqchip.chip.ioapic.redirtbl[0].fields.vector = 0x10;

	if (vmioctl(KVM_SET_IRQCHIP, &irqchip)) {
		ERROR << "Unable to configure IRQCHIP";
		return false;
	}

	DEBUG << "Creating IRQ eventfd";
	irq_fd = eventfd(0, EFD_NONBLOCK);
	if (irq_fd < 0) {
		ERROR << "Unable to create IRQ fd";
		return false;
	}

	struct kvm_irqfd irq;
	irq.fd = irq_fd;
	irq.flags = 0;
	irq.gsi = 0;
	irq.resamplefd = 0;

	DEBUG << "Attaching IRQ eventfd";
	if (vmioctl(KVM_IRQFD, &irq)) {
		ERROR << "Unable to assign IRQ fd";
		return false;
	}

	return true;
}

bool KVMGuest::prepare_guest_memory()
{
	// Allocate system memory
	sys_mem_rgn = alloc_guest_memory(SYSTEM_MEMORY_PHYS_BASE, SYSTEM_MEMORY_PHYS_SIZE);
	if (!sys_mem_rgn) {
		ERROR << "Unable to allocate system memory region";
		return false;
	}

	// Hack in the GDT
	uint64_t *gdt = (uint64_t *)&((uint8_t *)sys_mem_rgn->host_buffer)[0x100];

	*gdt++ = 0;				// NULL
	*gdt++ = 0x0020980000000000;		// KERNEL CS
	*gdt++ = 0x0000900000000000;		// KERNEL DS
	*gdt++ = 0x0020f80000000000;		// USER CS
	*gdt++ = 0x0000f00000000000;		// USER DS
	*gdt++ = 0x00408900020000d0;		// TSS
	*gdt = 2;

	// Hack in the TSS
	uint64_t *tss = (uint64_t *)&((uint8_t *)sys_mem_rgn->host_buffer)[0x200];

	tss[1] = 0xabcdef123456;
	tss[2] = 0;
	tss[19] = 0x1b;		// CS
	tss[20] = 0x23;		// SS
	tss[21] = 0x23;		// DS
	tss[22] = 0x23;		// FS
	tss[23] = 0x23;		// GS

	tss[25] = 208;

	// Allocate shared memory
	sh_mem_rgn = alloc_guest_memory(SHMEM_PHYS_BASE, SHMEM_PHYS_SIZE);
	if (!sh_mem_rgn) {
		ERROR << "Unable to allocate shared memory region";
		return false;
	}

	_shmem = (shmem_data *)sh_mem_rgn->host_buffer;
	_shmem->cpu_options.verify = verify_enabled();

	if (_shmem->cpu_options.verify) {
		_shmem->cpu_options.verify_id = verify_get_tid();
	}

	{
		// Try to obtain a free memory region slot.
		vm_mem_region *vrgn = get_mem_slot();
		if (vrgn == NULL)
			return NULL;

		// Fill in the KVM memory structure.
		vrgn->kvm.flags = 0;
		vrgn->kvm.guest_phys_addr = 0x60000000;
		vrgn->kvm.memory_size = 0x1000;

		// Allocate a userspace buffer for the region.
		vrgn->host_buffer = verify_get_shared_data();

		// Store the buffer address in the KVM memory structure.
		vrgn->kvm.userspace_addr = (uint64_t) vrgn->host_buffer;

		// Install the memory region into the guest.
		int rc = ioctl(fd, KVM_SET_USER_MEMORY_REGION, &vrgn->kvm);
		if (rc) {
			ERROR << "Unable to install verification memory";
			return NULL;
		}

		_shmem->verify_shm_data = (shmem_data::verify_shm_t *)(JIT_VIRT_BASE + JIT_SIZE);
	}

	// Allocate JIT memory
	jit_mem_rgn = alloc_guest_memory(JIT_PHYS_BASE, JIT_SIZE);
	if (!jit_mem_rgn) {
		ERROR << "Unable to allocate JIT memory region";
		return false;
	}

	jit().set_code_arena(jit_mem_rgn->host_buffer, jit_mem_rgn->kvm.memory_size);

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
#define PT_USER_ACCESS	(1ULL << 2)
#define PT_NO_EXECUTE	(1ULL << 63)

bool KVMGuest::install_initial_pgt()
{
	next_page = 0x3000;

	for (uint64_t va = 0, pa = 0; va < 0x200000; va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS);
	}

	return true;
}

bool KVMGuest::stage2_init(uint64_t& stack)
{
	// Map the ENGINE into memory
	for (uint64_t va = ENGINE_VIRT_BASE, pa = ENGINE_PHYS_BASE; va < (ENGINE_VIRT_BASE + ENGINE_SIZE); va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS);
	}

	// Map the SYSTEM DATA area into memory
	for (uint64_t va = DATA_VIRT_BASE, pa = DATA_PHYS_BASE; va < (DATA_VIRT_BASE + DATA_SIZE); va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS);
	}

	// Map the shared memory region into memory
	for (uint64_t va = SHMEM_VIRT_BASE, pa = SHMEM_PHYS_BASE; va < (SHMEM_VIRT_BASE + SHMEM_PHYS_SIZE); va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS);
	}

	// For now, put the stack starting at the end of shared memory
	stack = SHMEM_VIRT_BASE + SHMEM_PHYS_SIZE - 0x100;

	// Map the JIT memory region into memory
	for (uint64_t va = JIT_VIRT_BASE, pa = JIT_PHYS_BASE; va < (JIT_VIRT_BASE + JIT_SIZE); va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT);
	}

	// Map the verification page into memory
	map_page(JIT_VIRT_BASE + JIT_SIZE, 0x60000000, PT_PRESENT | PT_WRITABLE);

	// Map ALL guest physical memory, and mark it as present and writable for
	// use by the engine.
	for (uint64_t va = GUEST_PHYS_MEMORY_COPY_VIRT_BASE, pa = GUEST_PHYS_MEMORY_BASE; va < (GUEST_PHYS_MEMORY_COPY_VIRT_BASE + GUEST_PHYS_MEMORY_MAX_SIZE); va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT | PT_WRITABLE);
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

	pm_t pm = (pm_t)&base[0x2000];
	if (pm[pm_idx] == 0) {
		uint64_t new_page = alloc_page();
		bzero(&base[new_page], 0x1000);

		pm[pm_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
	}

	pdp_t pdp = (pdp_t)&base[pm[pm_idx] & ADDR_MASK];
	if (pdp[pdp_idx] == 0) {
		uint64_t new_page = alloc_page();
		bzero(&base[new_page], 0x1000);

		pdp[pdp_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
	}

	pd_t pd = (pd_t)&base[pdp[pdp_idx] & ADDR_MASK];
	if (pd[pd_idx] == 0) {
		uint64_t new_page = alloc_page();
		bzero(&base[new_page], 0x1000);

		pd[pd_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
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

bool KVMGuest::resolve_gpa(gpa_t gpa, void*& out_addr) const
{
	for (auto pmr : gpm) {
		if (gpa >= pmr.cfg->base_address() && gpa <= (pmr.cfg->base_address() + pmr.cfg->size())) {
			uint64_t offset = (uint64_t)gpa - (uint64_t)pmr.cfg->base_address();
			out_addr = (void *)((uint64_t)pmr.vmr->host_buffer + offset);
			return true;
		}
	}

	return false;
}
