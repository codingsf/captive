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

#define PT_PRESENT	(1 << 0)
#define PT_WRITABLE	(1 << 1)
#define PT_USER_ACCESS	(1 << 2)

#define BIOS_PHYS_BASE			0xf0000ULL

#define SYSTEM_DATA_PHYS_BASE		0x000000000ULL
#define SYSTEM_DATA_VIRT_BASE		0x200000000ULL
#define SYSTEM_DATA_SIZE		0x010000000ULL

#define ENGINE_PHYS_BASE		0x010000000ULL
#define ENGINE_VIRT_BASE		0xFFFFFFFF80000000ULL
#define ENGINE_SIZE			0x030000000ULL

#define ENGINE_HEAP_PHYS_BASE		0x040000000ULL
#define ENGINE_HEAP_VIRT_BASE		0x210000000ULL
#define ENGINE_HEAP_SIZE		0x050000000ULL

#define SHARED_MEM_PHYS_BASE		0x090000000ULL
#define SHARED_MEM_VIRT_BASE		0x260000000ULL
#define SHARED_MEM_SIZE			0x010000000ULL

#define JIT_PHYS_BASE			0x0a0000000ULL
#define JIT_VIRT_BASE			0x270000000ULL
#define JIT_SIZE			0x010000000ULL

#define GPM_PHYS_BASE			0x100000000ULL
#define GPM_VIRT_BASE			0x000000000ULL
#define GPM_COPY_VIRT_BASE		0x100000000ULL
#define GPM_SIZE			0x100000000ULL

#define VERIFY_PHYS_BASE		0x0b0000000ULL
#define VERIFY_VIRT_BASE		0x280000000ULL
#define VERIFY_SIZE			0x1000ULL

#define IR_BUFFER_OFFSET		0x10000
#define IR_BUFFER_SIZE			0x20000

#define IR_DESC_BUFFER_OFFSET		0x30000
#define IR_DESC_BUFFER_SIZE		0x10000

#define PRINTF_BUFFER_OFFSET		0x9000
#define PRINTF_BUFFER_SIZE		0x1000

struct {
	std::string name;
	uint64_t phys_base;
	uint64_t virt_base;
	uint64_t size;
	uint32_t prot_flags;
} guest_memory_regions[] = {
	{ .name = "system-data", .phys_base = SYSTEM_DATA_PHYS_BASE, .virt_base = SYSTEM_DATA_VIRT_BASE, .size = SYSTEM_DATA_SIZE, .prot_flags = PT_WRITABLE | PT_USER_ACCESS },
	{ .name = "engine",      .phys_base = ENGINE_PHYS_BASE,      .virt_base = ENGINE_VIRT_BASE,      .size = ENGINE_SIZE, .prot_flags = PT_WRITABLE | PT_USER_ACCESS },
	{ .name = "engine-heap", .phys_base = ENGINE_HEAP_PHYS_BASE, .virt_base = ENGINE_HEAP_VIRT_BASE, .size = ENGINE_HEAP_SIZE, .prot_flags = PT_WRITABLE | PT_USER_ACCESS },
	{ .name = "shared-mem",  .phys_base = SHARED_MEM_PHYS_BASE,  .virt_base = SHARED_MEM_VIRT_BASE,  .size = SHARED_MEM_SIZE, .prot_flags = PT_WRITABLE | PT_USER_ACCESS },
	{ .name = "jit-mem",     .phys_base = JIT_PHYS_BASE,         .virt_base = JIT_VIRT_BASE,         .size = JIT_SIZE, .prot_flags = PT_USER_ACCESS },
	/*{ .name = "gpm",         .phys_base = GPM_PHYS_BASE,         .virt_base = GPM_VIRT_BASE,         .size = GPM_SIZE },
	{ .name = "gpm-copy",    .phys_base = GPM_PHYS_BASE,         .virt_base = GPM_COPY_VIRT_BASE,    .size = GPM_SIZE },*/
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

	if (!loader.install((uint8_t *)get_phys_buffer(GPM_PHYS_BASE))) {
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

	uint64_t per_cpu_offset = 0x1000 + (0x1000 * next_cpu_id);

	// Locate the storage location for the Per-CPU data, and initialise the structure.
	PerCPUData *per_cpu_data = (PerCPUData *)get_phys_buffer(SHARED_MEM_PHYS_BASE + per_cpu_offset);
	per_cpu_data->async_action = 0;
	per_cpu_data->execution_mode = (uint32_t)config.execution_mode();
	per_cpu_data->guest_data = (PerGuestData *)SHARED_MEM_VIRT_BASE;
	per_cpu_data->insns_executed = 0;
	per_cpu_data->isr = 0;

	if (verify_enabled()) {
		per_cpu_data->verify_data = (VerificationData *)VERIFY_VIRT_BASE;
		per_cpu_data->verify_enabled = true;
		per_cpu_data->verify_tid = verify_get_tid();
	} else {
		per_cpu_data->verify_data = NULL;
		per_cpu_data->verify_enabled = false;
		per_cpu_data->verify_tid = 0;
	}

	KVMCpu *cpu = new KVMCpu(*this, config, next_cpu_id, cpu_fd, *per_cpu_data, SHARED_MEM_VIRT_BASE + per_cpu_offset);
	kvm_cpus.push_back(cpu);

	next_cpu_id++;
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
	/*DEBUG << CONTEXT(Guest) << "Creating IRQ chip";
	if (vmioctl(KVM_CREATE_IRQCHIP)) {
		ERROR << "Unable to create IRQCHIP";
		return false;
	}*/

	/*struct kvm_irqchip irqchip;
	bzero(&irqchip, sizeof(irqchip));

	irqchip.chip_id = 2;
	irqchip.chip.ioapic.base_address = 0x1000;
	irqchip.chip.ioapic.id = 0;
	irqchip.chip.ioapic.redirtbl[0].fields.vector = 0x10;

	if (vmioctl(KVM_SET_IRQCHIP, &irqchip)) {
		ERROR << "Unable to configure IRQCHIP";
		return false;
	}*/

	return true;
}

bool KVMGuest::prepare_guest_memory()
{
	// Allocate defined memory regions
	for (uint32_t i = 0; i < sizeof(guest_memory_regions) / sizeof(guest_memory_regions[0]); i++) {
		DEBUG << CONTEXT(Guest) << "Allocating memory region: " << guest_memory_regions[i].name
			<< ", phys-base=" << std::hex << guest_memory_regions[i].phys_base
			<< ", size=" << std::hex << guest_memory_regions[i].size;

		alloc_guest_memory(guest_memory_regions[i].phys_base, guest_memory_regions[i].size);
	}

	per_guest_data = (PerGuestData *)get_phys_buffer(SHARED_MEM_PHYS_BASE);
	assert(per_guest_data);

	per_guest_data->heap = (void *)ENGINE_HEAP_VIRT_BASE;
	per_guest_data->heap_size = ENGINE_HEAP_SIZE;
	per_guest_data->ir_buffer = (void *)(SHARED_MEM_VIRT_BASE + IR_BUFFER_OFFSET);
	per_guest_data->ir_buffer_size = IR_BUFFER_SIZE;
	per_guest_data->ir_desc_buffer = (void *)(SHARED_MEM_VIRT_BASE + IR_DESC_BUFFER_OFFSET);
	per_guest_data->ir_desc_buffer_size = IR_DESC_BUFFER_SIZE;
	per_guest_data->code_buffer = (void *)JIT_VIRT_BASE;
	per_guest_data->code_buffer_size = JIT_SIZE;
	per_guest_data->printf_buffer = (void *)(SHARED_MEM_VIRT_BASE + PRINTF_BUFFER_OFFSET);
	per_guest_data->printf_buffer_size = PRINTF_BUFFER_SIZE;

	if (!install_gdt()) {
		ERROR << CONTEXT(Guest) << "Unable to install GDT";
		return false;
	}

	if (!install_tss()) {
		ERROR << CONTEXT(Guest) << "Unable to install TSS";
		return false;
	}

	if (verify_enabled() && !prepare_verification_memory()) {
		ERROR << CONTEXT(Guest) << "Unable to prepare verification memory";
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
	if (!engine().install((uint8_t *)get_phys_buffer(ENGINE_PHYS_BASE))) {
		ERROR << "Unable to install execution engine";
		return false;
	}

	// Notify the JIT where the arena for generated code is.
	jit().set_code_arena(get_phys_buffer(JIT_PHYS_BASE), JIT_SIZE);
	jit().set_ir_buffer(get_phys_buffer(SHARED_MEM_PHYS_BASE + 0x10000), per_guest_data->ir_buffer_size);

	DEBUG << CONTEXT(Guest) << "Installing guest memory regions";
	for (auto& region : config().memory_regions) {
		struct vm_mem_region *vm_region = alloc_guest_memory(GPM_PHYS_BASE + region.base_address(), region.size());
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
	uint8_t *base = (uint8_t *) get_phys_buffer(BIOS_PHYS_BASE);

	unsigned int bios_size = &__bios_bin_end - &__bios_bin_start;
	memcpy(base, &__bios_bin_start, bios_size);

	return true;
}

bool KVMGuest::install_initial_pgt()
{
	per_guest_data->next_phys_page = 0x3000;

	for (uint64_t va = 0, pa = 0; va < 0x200000; va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS);
	}

	return true;
}

bool KVMGuest::install_gdt()
{
	// Hack in the GDT
	uint64_t *gdt = (uint64_t *)get_phys_buffer(0x100);

	*gdt++ = 0;				// NULL
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

	tss[0] = 0x210000000;

	return true;
}

bool KVMGuest::prepare_verification_memory()
{
	// Try to obtain a free memory region slot.
	vm_mem_region *vrgn = get_mem_slot();
	if (vrgn == NULL)
		return false;

	// Fill in the KVM memory structure.
	vrgn->kvm.flags = 0;
	vrgn->kvm.guest_phys_addr = VERIFY_PHYS_BASE;
	vrgn->kvm.memory_size = VERIFY_SIZE;

	// Use the verification memory buffer for this region
	vrgn->host_buffer = verify_get_shared_data();

	// Store the buffer address in the KVM memory structure.
	vrgn->kvm.userspace_addr = (uint64_t) vrgn->host_buffer;

	// Install the memory region into the guest.
	int rc = ioctl(fd, KVM_SET_USER_MEMORY_REGION, &vrgn->kvm);
	if (rc) {
		ERROR << "Unable to install verification memory";
		return NULL;
	}

	return true;
}


bool KVMGuest::stage2_init(uint64_t& stack)
{
	// Map defined memory regions
	for (uint32_t i = 0; i < sizeof(guest_memory_regions) / sizeof(guest_memory_regions[0]); i++) {
		DEBUG << CONTEXT(Guest) << "Mapping memory region: " << guest_memory_regions[i].name
			<< ", phys-base=" << std::hex << guest_memory_regions[i].phys_base
			<< ", virt-base=" << std::hex << guest_memory_regions[i].virt_base
			<< ", size=" << std::hex << guest_memory_regions[i].size;

		for (uint64_t va = guest_memory_regions[i].virt_base, pa = guest_memory_regions[i].phys_base; va < (guest_memory_regions[i].virt_base + guest_memory_regions[i].size); va += 0x1000, pa += 0x1000) {
			map_page(va, pa, PT_PRESENT | guest_memory_regions[i].prot_flags);
		}
	}

	// For now, put the stack starting at the end of shared memory
	stack = SHARED_MEM_VIRT_BASE + SHARED_MEM_SIZE;

	// Map the verification region (if enabled)
	if (verify_enabled()) {
		map_page(VERIFY_VIRT_BASE, VERIFY_PHYS_BASE, PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS);
	}

	// Map ALL guest physical memory, and mark it as present and writable for
	// use by the engine.
	for (uint64_t va = GPM_COPY_VIRT_BASE, pa = GPM_PHYS_BASE; va < (GPM_COPY_VIRT_BASE + GPM_SIZE); va += 0x1000, pa += 0x1000) {
		map_page(va, pa, PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS);
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

	DEBUG << CONTEXT(Guest) << "Allocated guest memory, gpa=" << std::hex << rgn->kvm.guest_phys_addr << ", size=" << std::hex << rgn->kvm.memory_size << ", hva=" << std::hex << rgn->host_buffer;
	return rgn;
}

void* KVMGuest::get_phys_buffer(uint64_t gpa)
{
	for (auto rgn : vm_mem_region_used) {
		if (gpa >= rgn->kvm.guest_phys_addr && gpa < (rgn->kvm.guest_phys_addr + rgn->kvm.memory_size)) {
			uint64_t offset = gpa - rgn->kvm.guest_phys_addr;

			return (void *)((uint64_t)rgn->host_buffer + offset);
		}
	}

	return NULL;
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

#define ADDR_MASK (~0xfffULL)
#define FLAGS_MASK (0xfffULL)

#define BITS(val, start, end) ((val >> start) & (((1 << (end - start + 1)) - 1)))

void KVMGuest::map_page(uint64_t va, uint64_t pa, uint32_t flags)
{
	uint16_t pm_idx = BITS(va, 39, 47);
	uint16_t pdp_idx = BITS(va, 30, 38);
	uint16_t pd_idx = BITS(va, 21, 29);
	uint16_t pt_idx = BITS(va, 12, 20);

	pm_t pm = (pm_t)get_phys_buffer(0x2000);
	if (pm[pm_idx] == 0) {
		uint64_t new_page = alloc_page();
		bzero(get_phys_buffer(new_page), 0x1000);

		pm[pm_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
	}

	pdp_t pdp = (pdp_t)get_phys_buffer(pm[pm_idx] & ADDR_MASK);
	if (pdp[pdp_idx] == 0) {
		uint64_t new_page = alloc_page();
		bzero(get_phys_buffer(new_page), 0x1000);

		pdp[pdp_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
	}

	pd_t pd = (pd_t)get_phys_buffer(pdp[pdp_idx] & ADDR_MASK);
	if (pd[pd_idx] == 0) {
		uint64_t new_page = alloc_page();
		bzero(get_phys_buffer(new_page), 0x1000);

		pd[pd_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
	}

	pt_t pt = (pt_t)get_phys_buffer(pd[pd_idx] & ADDR_MASK);
	pt[pt_idx] = (pa & ADDR_MASK) | ((uint64_t)flags & FLAGS_MASK);

	/*DEBUG << "Map Page VA=" << std::hex << va
		<< ", PA=" << pa
		<< ", PM[2000]=" << (uint32_t)pm_idx
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

void KVMGuest::do_guest_printf()
{
	fprintf(stderr, "%s", (char *)get_phys_buffer(SHARED_MEM_PHYS_BASE + PRINTF_BUFFER_OFFSET));
}
