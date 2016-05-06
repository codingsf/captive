#include <hypervisor/kvm/kvm.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/cpu.h>
#include <hypervisor/config.h>
#include <loader/loader.h>
#include <engine/engine.h>
#include <devices/device.h>
#include <platform/platform.h>
#include <shmem.h>

#include <thread>
#include <pthread.h>

#ifdef HUGETLB
extern "C" {
#include <hugetlbfs.h>
}
#endif

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/signal.h>
#include <linux/kvm.h>

#include <iomanip>

#define FAST_DEVICE_ACCESS
//#define GUEST_EVENTS

USE_CONTEXT(Guest);

using namespace captive::engine;
using namespace captive::hypervisor;
using namespace captive::hypervisor::kvm;

#define VERBOSE_ENABLED		false

#define PT_PRESENT			(1 << 0)
#define PT_WRITABLE			(1 << 1)
#define PT_USER_ACCESS		(1 << 2)
#define PT_WRITE_THROUGH	(1 << 3)
#define PT_CACHE_DISABLED	(1 << 4)
#define PT_DIRTY			(1 << 6)
#define PT_HUGE_PAGE		(1 << 7)
#define PT_GLOBAL			(1 << 8)

#define HOST_BIOS_BASE				(HOST_CODE_BASE + 0xf0000ULL)

#define DEFAULT_NR_SLOTS		32

extern char __bios_bin_start, __bios_bin_end;

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
	if (initialised()) {
		release_all_guest_memory();
		cleanup_event_loop();
	}

	DEBUG << CONTEXT(Guest) << "Closing KVM VM";
	close(fd);
}

bool KVMGuest::init()
{
	if (!Guest::init())
		return false;
	
	if (!prepare_event_loop())
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
	
	for (auto core : platform().config().cores) {
		if (!create_cpu(core)) {
			return false;
		}
	}

	_initialised = true;
	return true;
}

static bool stop_callback(int fd, bool is_input, void *p)
{
	return false;
}

bool KVMGuest::intr_callback(int fd, bool is_input, void *p)
{
	KVMGuest *g = (KVMGuest *)p;
	
	g->platform().dump();
	
	g->kvm_cpus.front()->interrupt(4);
	return true;
}

bool KVMGuest::prepare_event_loop()
{
	stopfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (stopfd < 0) {
		ERROR << CONTEXT(Guest) << "Unable to create stop eventfd";
		return false;
	}

	intrfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (intrfd < 0) {
		ERROR << CONTEXT(Guest) << "Unable to create interrupt eventfd";
		close(stopfd);
		return false;
	}

	epollfd = epoll_create1(0);
	if (epollfd < 0) {
		ERROR << CONTEXT(Guest) << "Unable to create epollfd";
		close(intrfd);
		close(stopfd);
		return false;
	}
	
	if (!attach_event(stopfd, stop_callback, true, false, NULL)) {
		close(intrfd);
		close(stopfd);
		return false;
	}

	if (!attach_event(intrfd, intr_callback, true, false, this)) {
		close(intrfd);
		close(stopfd);
		return false;
	}
	
	return true;
}

bool KVMGuest::attach_event(int fd, event_callback_t cb, bool input, bool output, void *data)
{
	struct event_loop_event *loop_event = new struct event_loop_event();
	loop_event->fd = fd;
	loop_event->cb = cb;
	loop_event->data = data;
	
	struct epoll_event evt;
	evt.data.ptr = (void *)loop_event;
	evt.events = EPOLLET;
	
	if (input) evt.events |= EPOLLIN;
	if (output) evt.events |= EPOLLOUT;
	
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &evt) < 0) {
		ERROR << CONTEXT(Guest) << "Unable to attach event";
		return false;
	}
	
	return true;
}

void KVMGuest::cleanup_event_loop()
{
	close(epollfd);
	close(stopfd);
}

bool KVMGuest::load(loader::Loader& loader)
{
	if (!initialised()) {
		ERROR << "KVM guest is not yet initialised";
		return false;
	}
	
	DEBUG << CONTEXT(Guest) << "Installing Loader";
	if (!loader.install((uint8_t *)HOST_GPM_BASE)) {
		ERROR << CONTEXT(Guest) << "Loader failed to install";
		return false;
	}

	return true;
}

void KVMGuest::guest_entrypoint(gpa_t entrypoint)
{
	per_guest_data->entrypoint = entrypoint;
}

#ifndef NDEBUG
std::map<captive::devices::Device *, uint64_t> device_reads, device_writes;
#endif

bool KVMGuest::run()
{
#ifdef FAST_DEVICE_ACCESS
	// Create the device thread
	std::thread device_thread(device_thread_proc, (KVMGuest *)this);
#endif
	
	// Create and run each core thread.
	std::list<std::thread *> core_threads;
	for (auto core : kvm_cpus) {
		auto core_thread = new std::thread(core_thread_proc, core);
		core_threads.push_back(core_thread);
	}
	
#define MAX_EVENTS	8
	
	struct epoll_event evts[MAX_EVENTS];
	
	bool terminate = false;
	while (!terminate) {
		int count = epoll_wait(epollfd, evts, MAX_EVENTS, -1);
		
		if (count < 0) {
			if (errno != EINTR) {
				ERROR << CONTEXT(Guest) << "epoll error: " << LAST_ERROR_TEXT;
				break;
			}
		} else {
			for (int i = 0; i < count; i++) {
				if (evts[i].data.ptr) {
					const struct event_loop_event *loop_event = (const struct event_loop_event *)evts[i].data.ptr;
					terminate = !loop_event->cb(loop_event->fd, (evts[i].events & EPOLLIN), loop_event->data);
				}
			}
		}
	}
		
	// Signal each core to stop.
	for (auto core : kvm_cpus) {
		core->stop();
	}
	
	// Wait for core threads to terminate.
	for (auto thread : core_threads) {
		if (thread->joinable()) thread->join();
	}
	
#ifndef NDEBUG
	fprintf(stderr, "device reads:\n");
	for (auto devread : device_reads) {
		fprintf(stderr, "  %s: %lu\n", devread.first->name().c_str(), devread.second);
	}
	
	fprintf(stderr, "device writes:\n");
	for (auto devwrite : device_writes) {
		fprintf(stderr, "  %s: %lu\n", devwrite.first->name().c_str(), devwrite.second);
	}
#endif
	
#ifdef FAST_DEVICE_ACCESS
	// Shutdown the device thread
	per_guest_data->fast_device.operation = FAST_DEV_OP_QUIT;
	captive::lock::barrier_wait(&per_guest_data->fast_device.hypervisor_barrier, FAST_DEV_GUEST_TID);
	
	if (device_thread.joinable()) {
		device_thread.join();
	}
#endif
	
	return true;
}

void KVMGuest::stop()
{
	uint64_t value = 1;
	write(stopfd, &value, sizeof(value));
}

void KVMGuest::debug_interrupt(int code)
{
	uint64_t value = 1;
	write(intrfd, &value, sizeof(value));
}

void KVMGuest::device_thread_proc(KVMGuest *guest)
{
	pthread_setname_np(pthread_self(), "dev-comm");
	
	PerGuestData *pgd = guest->per_guest_data;
	while (true) {
		captive::lock::barrier_wait(&pgd->fast_device.hypervisor_barrier, FAST_DEV_HYPERVISOR_TID);
		if (pgd->fast_device.operation == FAST_DEV_OP_QUIT) break;
		
		uint64_t base_addr;
		captive::devices::Device *device = guest->lookup_device(pgd->fast_device.address, base_addr);
		
		if (!device) exit(0);
		
		uint64_t offset = pgd->fast_device.address - base_addr;
		if (pgd->fast_device.operation == FAST_DEV_OP_WRITE) {
#ifndef NDEBUG
			device_writes[device]++;
#endif
			device->write(offset, pgd->fast_device.size, pgd->fast_device.value);
		} else if (pgd->fast_device.operation == FAST_DEV_OP_READ) {
#ifndef NDEBUG
			device_reads[device]++;
#endif
			uint64_t new_value;
			device->read(offset, pgd->fast_device.size, new_value);
			pgd->fast_device.value = new_value;
		} else {
			break;
		}
		
		captive::lock::barrier_wait(&pgd->fast_device.guest_barrier, FAST_DEV_HYPERVISOR_TID);
	}
}

void KVMGuest::event_thread_proc(KVMCpu *core)
{
	pthread_setname_np(pthread_self(), "events");
		
	FILE *f = fopen("./core.events", "wt");
	
	volatile uint64_t *ring = (volatile uint64_t *)core->per_cpu_data().event_ring;
	
	uint64_t last = ring[0];
	while (true) {
		uint64_t next = ring[0];
		
		if (next != last) {
			for (; last != next; last++, last &= 0xff) {
				uint64_t entry = ring[last];
				fprintf(f, "[%s] pc=%08x, addr=%08x\n", !!(entry & 1) ? "W" : "R", entry & 0xfffffffe, (entry >> 32));
			}
		}
	}
	
	fflush(f);
	fclose(f);
}

void KVMGuest::core_thread_proc(KVMCpu *core)
{
#ifdef GUEST_EVENTS
	// Create the device thread
	std::thread event_thread(event_thread_proc, (KVMCpu *)core);
#endif
	
	std::string thread_name = "core-" + std::to_string(core->id());
	pthread_setname_np(pthread_self(), thread_name.c_str());
	
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	
	pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
	
	Guest::current_core = core;
	core->run();
	core->instrument_dump();
	
	core->owner().stop();
}

bool KVMGuest::create_cpu(const GuestCPUConfiguration& config)
{
	if (!config.validate()) {
		ERROR << "Invalid CPU configuration";
		return false;
	}

	DEBUG << CONTEXT(Guest) << "Creating KVM VCPU";
	int cpu_fd = ioctl(fd, KVM_CREATE_VCPU, next_cpu_id);
	if (cpu_fd < 0) {
		ERROR << "Failed to create KVM VCPU: " << LAST_ERROR_TEXT;
		return false;
	}
	
	struct kvm_irqchip irqchip;
	bzero(&irqchip, sizeof(irqchip));
	irqchip.chip_id = 2;

	if (vmioctl(KVM_GET_IRQCHIP, &irqchip)) {
		ERROR << "Unable to retrieve IRQCHIP";
		return false;
	}
			
	irqchip.chip.ioapic.redirtbl[(next_cpu_id * 2) + 16].fields.vector = 0x30;
	irqchip.chip.ioapic.redirtbl[(next_cpu_id * 2) + 16].fields.trig_mode = 1;
	irqchip.chip.ioapic.redirtbl[(next_cpu_id * 2) + 16].fields.mask = 0;
	irqchip.chip.ioapic.redirtbl[(next_cpu_id * 2) + 16].fields.dest_id = next_cpu_id;
	
	irqchip.chip.ioapic.redirtbl[(next_cpu_id * 2) + 17].fields.vector = 0x31;
	irqchip.chip.ioapic.redirtbl[(next_cpu_id * 2) + 17].fields.trig_mode = 1;
	irqchip.chip.ioapic.redirtbl[(next_cpu_id * 2) + 17].fields.mask = 0;
	irqchip.chip.ioapic.redirtbl[(next_cpu_id * 2) + 17].fields.dest_id = next_cpu_id;

	DEBUG << CONTEXT(Guest) << "Configuring IRQ chip";
	if (vmioctl(KVM_SET_IRQCHIP, &irqchip)) {
		ERROR << "Unable to configure IRQCHIP";
		return false;
	}

	// Locate the storage location for the Per-CPU data, and initialise the structure.
	PerCPUData *per_cpu_data = (PerCPUData *)(HOST_SYS_GUEST_DATA + (0x100 * (next_cpu_id + 1)));
	per_cpu_data->id = next_cpu_id;
	per_cpu_data->guest_data = (PerGuestData *)GUEST_SYS_GUEST_DATA_VIRT;
	per_cpu_data->event_ring = (void *)GUEST_SYS_EVENT_RING_VIRT;
	per_cpu_data->async_action = 0;
	per_cpu_data->execution_mode = 0;
	per_cpu_data->insns_executed = 0;
	per_cpu_data->interrupts_taken = 0;
	per_cpu_data->verbose_enabled = false;
	per_cpu_data->watchpoint = 0;
	per_cpu_data->interrupt_pending = NULL;

	KVMCpu *cpu = new KVMCpu(next_cpu_id, *this, config, cpu_fd, per_cpu_data);
	if (!cpu->init()) {
		delete cpu;
		return false;
	}
	
	kvm_cpus.push_back(cpu);

	next_cpu_id++;
	return true;
}

bool KVMGuest::attach_guest_devices()
{
	for (const auto& device : platform().config().devices) {
		DEBUG << CONTEXT(Guest) << "Attaching device " << device.device().name() << " @ " << std::hex << device.base_address();

		device.device().attach(*this);

		dev_desc desc;
		desc.cfg = &device;
		desc.dev = &device.device();

		devices[device.base_address()] = desc;
	}

	return true;
}

captive::devices::Device *KVMGuest::lookup_device(uint64_t addr, uint64_t& base_addr)
{
	for (auto desc : devices) {
		if (addr >= desc.first && addr < desc.first + desc.second.dev->size()) {
			base_addr = desc.first;
			return desc.second.dev;
		}
	}
	
	/*auto candidate = devices.lower_bound(addr);
	if (candidate->first != addr) candidate--;
	if (candidate == devices.end()) return NULL;
	
	//fprintf(stderr, "*** CANDIDATE %lx for %lx\n", candidate->first, addr);

	if ((addr >= candidate->first) && (addr < (candidate->first + candidate->second.dev->size()))) {
		base_addr = candidate->first;
		
		return candidate->second.dev;
	}

	//fprintf(stderr, "*** NO SUCH DEVICE\n");*/
	return NULL;
}

bool KVMGuest::prepare_guest_irq()
{
	DEBUG << CONTEXT(Guest) << "Creating IRQ chip";
	if (vmioctl(KVM_CREATE_IRQCHIP)) {
		ERROR << "Unable to create IRQCHIP";
		return false;
	}

	return true;
}

bool KVMGuest::prepare_guest_memory()
{
	// Prepare Guest Physical Memory
	for (auto& region : platform().config().memory_regions) {
		void *hva = (void *)((uint64_t)HOST_GPM_BASE + region.base_address());
		
		DEBUG << CONTEXT(Guest) << "Installing GPM region, base=" << std::hex << region.base_address() << ", size=" << region.size() << ", hva=" << hva;
		
		struct vm_mem_region *vm_region = alloc_guest_memory(GUEST_GPM_PHYS_BASE + region.base_address(), region.size(), 0, hva);
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
	
	struct vm_mem_region *vm_region_check;

	// Prepare Engine Heap
	DEBUG << CONTEXT(Guest) << "Installing heap, base=" << std::hex << GUEST_HEAP_PHYS_BASE << ", size=" << HEAP_SIZE << ", hva=" << HOST_HEAP_BASE;
	vm_region_check = alloc_guest_memory(GUEST_HEAP_PHYS_BASE, HEAP_SIZE, 0, (void *)HOST_HEAP_BASE);
	if (!vm_region_check) {
		release_all_guest_memory();
		ERROR << "Unable to allocate guest memory heap region";
		return false;
	}

	// Prepare Engine Code
	DEBUG << CONTEXT(Guest) << "Installing code, base=" << std::hex << GUEST_CODE_PHYS_BASE << ", size=" << CODE_SIZE << ", hva=" << HOST_CODE_BASE;
	vm_region_check = alloc_guest_memory(GUEST_CODE_PHYS_BASE, CODE_SIZE, 0, (void *)HOST_CODE_BASE);
	if (!vm_region_check) {
		release_all_guest_memory();
		ERROR << "Unable to allocate guest memory code region";
		return false;
	}
	
	// Prepare System Data
	DEBUG << CONTEXT(Guest) << "Installing code, base=" << std::hex << GUEST_SYS_PHYS_BASE << ", size=" << SYS_SIZE << ", hva=" << HOST_SYS_BASE;
	vm_region_check = alloc_guest_memory(GUEST_SYS_PHYS_BASE, SYS_SIZE, 0, (void *)HOST_SYS_BASE);
	if (!vm_region_check) {
		release_all_guest_memory();
		ERROR << "Unable to allocate guest memory sys region";
		return false;
	}

	// Next initial page-table is AFTER the PML
	next_init_pgt_page = GUEST_SYS_INIT_PGT_PHYS + 0x1000;
	DEBUG << CONTEXT(Guest) << "Next phys page for init pgt = " << std::hex << next_init_pgt_page;
	
	per_guest_data = (PerGuestData *)(HOST_SYS_GUEST_DATA);

	DEBUG << CONTEXT(Guest) << "Initialising per-guest data structure @ " << std::hex << per_guest_data;
	per_guest_data->fast_device.address = 0;
	per_guest_data->fast_device.value = 0;
	per_guest_data->fast_device.size = 0;
	per_guest_data->fast_device.operation = 0;
	captive::lock::barrier_init(&per_guest_data->fast_device.hypervisor_barrier);
	captive::lock::barrier_init(&per_guest_data->fast_device.guest_barrier);
	
	per_guest_data->printf_buffer = GUEST_SYS_PRINTF_VIRT;
	per_guest_data->heap_virt_base = GUEST_HEAP_VIRT_BASE;
	per_guest_data->heap_phys_base = GUEST_HEAP_PHYS_BASE;
	per_guest_data->heap_size = HEAP_SIZE;

	if (!install_gdt()) {
		ERROR << CONTEXT(Guest) << "Unable to install GDT";
		return false;
	}

	if (!install_tss()) {
		ERROR << CONTEXT(Guest) << "Unable to install TSS";
		return false;
	}
	
	map_pages(GUEST_GPM_VIRT_BASE, GUEST_GPM_PHYS_BASE, GPM_SIZE, PT_WRITABLE | PT_USER_ACCESS | PT_GLOBAL, false);
	map_pages(GUEST_HEAP_VIRT_BASE, GUEST_HEAP_PHYS_BASE, HEAP_SIZE, PT_WRITABLE | PT_USER_ACCESS | PT_GLOBAL);
	map_pages(0x680000000000, GUEST_CODE_PHYS_BASE, CODE_SIZE, PT_WRITABLE | PT_USER_ACCESS | PT_GLOBAL);
	map_pages(GUEST_CODE_VIRT_BASE, GUEST_CODE_PHYS_BASE, CODE_SIZE, PT_WRITABLE | PT_USER_ACCESS | PT_GLOBAL);
	map_pages(GUEST_SYS_VIRT_BASE, GUEST_SYS_PHYS_BASE, SYS_SIZE, PT_WRITABLE | PT_USER_ACCESS | PT_GLOBAL);

	// Map the LAPIC
	map_page(GUEST_LAPIC_VIRT_BASE, GUEST_LAPIC_PHYS_BASE, PT_PRESENT | PT_WRITABLE | PT_GLOBAL);
	
	// Install the engine code
	if (!engine().install((uint8_t *)HOST_CODE_BASE)) {
		ERROR << "Unable to install execution engine";
		return false;
	}

	return true;
}

bool KVMGuest::install_gdt()
{
	// Hack in the GDT
	uint64_t *gdt = (uint64_t *)HOST_SYS_GDT;
	
	DEBUG << CONTEXT(Guest) << "Installing GDT @ " << std::hex << gdt;

	*gdt++ = 0;							// NULL				 0
	*gdt++ = 0x0020980000000000;		// KERNEL CS		08
	*gdt++ = 0x0000920000000000;		// KERNEL DS		10
	*gdt++ = 0x0020f80000000000;		// USER CS			18
	*gdt++ = 0x0000f20000000000;		// USER DS			20
	*gdt++ = 0x40408900200000d0;		// TSS				28
	*gdt++ = 0x6800;					//					30
	
	return true;
}

bool KVMGuest::install_tss()
{
	// Hack in the TSS
	uint64_t *tss = (uint64_t *)(HOST_SYS_TSS + 4);
	
	DEBUG << CONTEXT(Guest) << "Installing TSS @ " << std::hex << tss;

	tss[0] = GUEST_SYS_KERNEL_STACK_TOP;

	return true;
}

void KVMGuest::dump_memory()
{
	for (auto rgn : vm_mem_region_used) {
		DEBUG << CONTEXT(Guest) << "Memory Region" << std::endl
		<< "================================" << std::endl
		<< "GUEST PHYS-BASE: " << std::hex << std::setw(16) << rgn->kvm.guest_phys_addr << std::endl
		<< " HOST VIRT-BASE: " << std::hex << std::setw(16) << rgn->kvm.userspace_addr << std::endl
		<< "    HOST BUFFER: " << std::hex << std::setw(16) << rgn->host_buffer << std::endl
		<< "           SIZE: " << std::hex << std::setw(16) << rgn->kvm.memory_size << std::endl
		<< "           SLOT: " << std::dec << rgn->kvm.slot << std::endl
		<< "          FLAGS: " << std::dec << rgn->kvm.flags;
	}
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
	
#ifdef HUGETLB
	mmap_flags |= MAP_HUGETLB;
#endif

	if (fixed_addr) {
		mmap_flags |= MAP_FIXED;
	} else {
		assert(false);
	}

	DEBUG << CONTEXT(Guest) << "MMAP @ " << std::hex << fixed_addr;
	rgn->host_buffer = mmap(fixed_addr, size, PROT_READ | PROT_WRITE | PROT_EXEC, mmap_flags, -1, 0);
	if (rgn->host_buffer == MAP_FAILED) {
		put_mem_slot(rgn);

		ERROR << "Unable to allocate memory";
		return NULL;
	}
		
	if (fixed_addr && rgn->host_buffer != fixed_addr) {
		munmap(rgn->host_buffer, rgn->kvm.memory_size);
		put_mem_slot(rgn);
		
		ERROR << "Unable to allocate fixed memory";
		return NULL;
	}
	
	madvise(rgn->host_buffer, size, MADV_MERGEABLE);

	// Store the buffer address in the KVM memory structure.
	rgn->kvm.userspace_addr = (uint64_t) rgn->host_buffer;

	DEBUG << CONTEXT(Guest) << "Installing memory region into guest gpa=" << std::hex << rgn->kvm.guest_phys_addr;

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

void KVMGuest::map_pages(uint64_t va_base, uint64_t pa_base, uint64_t size, uint32_t flags, bool use_huge_pages)
{
	if (use_huge_pages && (size % 0x200000) == 0) {
		for (uint64_t va = va_base, pa = pa_base; va < (va_base + size); va += 0x200000, pa += 0x200000) {
			map_huge_page(va, pa, PT_PRESENT | flags);
		}
	} else {
		for (uint64_t va = va_base, pa = pa_base; va < (va_base + size); va += 0x1000, pa += 0x1000) {
			map_page(va, pa, PT_PRESENT | flags);
		}
	}
}

#define ADDR_MASK (~0xfffULL)
#define FLAGS_MASK (0xfffULL)

#define BITS(val, start, end) ((val >> start) & (((1 << (end - start + 1)) - 1)))

void KVMGuest::map_page(uint64_t va, uint64_t pa, uint32_t flags)
{
	// VA must be page-aligned
	assert((va & 0xfff) == 0);

	// PA must be page-aligned
	assert((pa & 0xfff) == 0);

	uint16_t pm_idx = BITS(va, 39, 47);
	uint16_t pdp_idx = BITS(va, 30, 38);
	uint16_t pd_idx = BITS(va, 21, 29);
	uint16_t pt_idx = BITS(va, 12, 20);

	assert(pm_idx < 0x200);
	assert(pdp_idx < 0x200);
	assert(pd_idx < 0x200);
	assert(pt_idx < 0x200);

	pm_t pm = (pm_t)sys_guest_phys_to_host_virt(GUEST_SYS_INIT_PGT_PHYS);
	if (pm[pm_idx] == 0) {
		uint64_t new_page = alloc_init_pgt_page();

		pm[pm_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
	}

	pdp_t pdp = (pdp_t)sys_guest_phys_to_host_virt(pm[pm_idx] & ADDR_MASK);
	if (pdp[pdp_idx] == 0) {
		uint64_t new_page = alloc_init_pgt_page();

		pdp[pdp_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
	}

	pd_t pd = (pd_t)sys_guest_phys_to_host_virt(pdp[pdp_idx] & ADDR_MASK);
	if (pd[pd_idx] == 0) {
		uint64_t new_page = alloc_init_pgt_page();

		pd[pd_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
	}

	assert(!(pd[pd_idx] & PT_HUGE_PAGE));

	pt_t pt = (pt_t)sys_guest_phys_to_host_virt(pd[pd_idx] & ADDR_MASK);
	pt[pt_idx] = (pa & ADDR_MASK) | ((uint64_t)flags & FLAGS_MASK);

	DEBUG << CONTEXT(Guest) << "Map Page VA=" << std::hex << va
		<< ", PA=" << pa
		<< ", PM @ " << GUEST_SYS_INIT_PGT_PHYS
		<< ", PM[" << (uint32_t)pm_idx << "] = " << pm[pm_idx]
		<< ", PDP[" << (uint32_t)pdp_idx << "]=" << pdp[pdp_idx]
		<< ", PD[" << (uint32_t)pd_idx << "]=" << pd[pd_idx]
		<< ", PT[" << (uint32_t)pt_idx << "]=" << pt[pt_idx];
}

void KVMGuest::map_huge_page(uint64_t va, uint64_t pa, uint32_t flags)
{
	// VA must be huge-page-aligned
	assert((va & 0x1fffff) == 0);

	uint16_t pm_idx = BITS(va, 39, 47);
	uint16_t pdp_idx = BITS(va, 30, 38);
	uint16_t pd_idx = BITS(va, 21, 29);

	pm_t pm = (pm_t)sys_guest_phys_to_host_virt(GUEST_SYS_INIT_PGT_PHYS);
	if (pm[pm_idx] == 0) {
		uint64_t new_page = alloc_init_pgt_page();
		pm[pm_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
	}

	pdp_t pdp = (pdp_t)sys_guest_phys_to_host_virt(pm[pm_idx] & ADDR_MASK);
	if (pdp[pdp_idx] == 0) {
		uint64_t new_page = alloc_init_pgt_page();
		pdp[pdp_idx] = new_page | PT_PRESENT | PT_WRITABLE | PT_USER_ACCESS;
	}

	pd_t pd = (pd_t)sys_guest_phys_to_host_virt(pdp[pdp_idx] & ADDR_MASK);

	assert(pd[pd_idx] == 0);

	flags |= PT_HUGE_PAGE;
	pd[pd_idx] = (pa & ADDR_MASK) | ((uint64_t)flags & FLAGS_MASK);

	DEBUG << CONTEXT(Guest) << "Map Huge Page VA=" << std::hex << va
		<< ", PA=" << pa
		<< ", PM @ " << GUEST_SYS_INIT_PGT_PHYS
		<< ", PM[" << (uint32_t)pm_idx << "] = " << pm[pm_idx]
		<< ", PDP[" << (uint32_t)pdp_idx << "]=" << pdp[pdp_idx]
		<< ", PD[" << (uint32_t)pd_idx << "]=" << pd[pd_idx];
}

void* KVMGuest::sys_guest_phys_to_host_virt(uint64_t addr)
{
	assert(addr > GUEST_SYS_PHYS_BASE && addr < GUEST_SYS_PHYS_BASE + SYS_SIZE);
	return (void *)(HOST_SYS_BASE + (addr & (SYS_SIZE - 1)));
}


bool KVMGuest::resolve_gpa(gpa_t gpa, void*& out_addr) const
{
	out_addr = (void *)(HOST_GPM_BASE | (uint64_t)gpa);
	return true;
	
	/*for (auto pmr : gpm) {
		if (gpa >= pmr.cfg->base_address() && gpa < (pmr.cfg->base_address() + pmr.cfg->size())) {
			uint64_t offset = (uint64_t)gpa - (uint64_t)pmr.cfg->base_address();
			out_addr = (void *)((uint64_t)pmr.vmr->host_buffer + offset);
			return true;
		}
	}

	return false;*/
}

void KVMGuest::do_guest_printf()
{
	fprintf(stderr, "%s", (const char *)HOST_SYS_PRINTF);
}
