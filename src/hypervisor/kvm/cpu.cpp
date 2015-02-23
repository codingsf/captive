#include <engine/engine.h>
#include <hypervisor/kvm/cpu.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/kvm.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <devices/device.h>
#include <devices/irq/irq-controller.h>
#include <sys/eventfd.h>
#include <sys/signal.h>
#include <shmem.h>

using namespace captive::hypervisor::kvm;

KVMCpu::KVMCpu(KVMGuest& owner, const GuestCPUConfiguration& config, int id, int fd, int irq_fd) : CPU(owner, config), _initialised(false), _id(id), fd(fd), irqfd(irq_fd), cpu_run_struct(NULL), cpu_run_struct_size(0)
{

}

KVMCpu::~KVMCpu()
{
	if (initialised()) {
		close(irqfd);
		munmap(cpu_run_struct, cpu_run_struct_size);
	}

	DEBUG << "Closing KVM VCPU fd";
	close(fd);
}

bool KVMCpu::init()
{
	if (initialised()) {
		ERROR << "KVM CPU already initialised";
		return false;
	}

	if (!CPU::init())
		return false;

	// Attach the CPU IRQ controller
	owner().config().cpu_irq_controller->attach(*this);

	cpu_run_struct_size = ioctl(((KVM &)((KVMGuest &)owner()).owner()).kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if ((int)cpu_run_struct_size == -1) {
		ERROR << "Unable to ascertain size of CPU run structure";
		return false;
	}

	DEBUG << "Mapping CPU run structure size=" << std::hex << cpu_run_struct_size;
	cpu_run_struct = (struct kvm_run *)mmap(NULL, cpu_run_struct_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (cpu_run_struct == MAP_FAILED) {
		ERROR << "Unable to mmap CPU run struct";
		return false;
	}

	_initialised = true;
	return true;
}

void KVMCpu::interrupt(uint32_t code)
{
	/*struct kvm_interrupt irq;
	irq.irq = code + 32;

	if (vmioctl(KVM_INTERRUPT, &irq)) {
		WARNING << "Guest interrupt assertion failed";
	}*/

	//DEBUG << "Asserting a KVM interrupt: fd=" << irqfd << ", code=" << std::hex << code;

	/*uint64_t data = 1;
	if (write(irqfd, &data, sizeof(data)) != 8) {
		WARNING << "Unable to assert KVM interrupt, eventfd value not written";
	}*/

	((KVMGuest &)owner()).shmem_region()->asynchronous_action_pending = 1;
}

static KVMCpu *signal_cpu;

static void handle_signal(int signo)
{
	if (signo == SIGUSR1) {
		((KVMGuest &)signal_cpu->owner()).shmem_region()->asynchronous_action_pending = 2;
	} else if (signo == SIGUSR2) {
		((KVMGuest &)signal_cpu->owner()).shmem_region()->asynchronous_action_pending = 3;
	}
}

bool KVMCpu::run()
{
	KVMGuest& kvm_guest = (KVMGuest &)owner();

	bool run_cpu = true;
	int rc;

	if (!initialised()) {
		ERROR << "CPU not initialised";
		return false;
	}

	signal_cpu = this;
	signal(SIGUSR1, handle_signal);
	signal(SIGUSR2, handle_signal);

	struct kvm_sregs sregs;
	vmioctl(KVM_GET_SREGS, &sregs);
	sregs.cs.base = 0xf0000;
	vmioctl(KVM_SET_SREGS, &sregs);

	DEBUG << "Running CPU " << id();
	do {
		rc = vmioctl(KVM_RUN);
		if (rc < 0) {
			if (errno == EINTR) continue;

			ERROR << "Unable to run VCPU: " << LAST_ERROR_TEXT;
			return false;
		}

		switch (cpu_run_struct->exit_reason) {
		case KVM_EXIT_DEBUG:
			DEBUG << "DEBUG";
			dump_regs();
			break;

		case KVM_EXIT_IO:
			if (cpu_run_struct->io.port == 0xff) {
				struct kvm_regs regs;
				vmioctl(KVM_GET_REGS, &regs);

				DEBUG << "Handling hypercall " << std::hex << regs.rax;
				run_cpu = handle_hypercall(regs.rax);
				if (!run_cpu) {
					ERROR << "Unhandled hypercall " << std::hex << regs.rax;
				}
			} else if (cpu_run_struct->io.port == 0xfe) {
				struct kvm_regs regs;
				vmioctl(KVM_GET_REGS, &regs);

				fprintf(stderr, "%c", (char)regs.rax & 0xff);
			} else {
				run_cpu = false;
				DEBUG << "EXIT IO "
					"port=" << std::hex << cpu_run_struct->io.port << ", "
					"data offset=" << std::hex << cpu_run_struct->io.data_offset << ", "
					"count=" << std::hex << cpu_run_struct->io.count;
			}
			break;
		case KVM_EXIT_MMIO:
			if (cpu_run_struct->mmio.phys_addr > 0x100000000 && cpu_run_struct->mmio.phys_addr < 0x200000000) {
				uint64_t converted_pa = cpu_run_struct->mmio.phys_addr - 0x100000000;
				devices::Device *dev = kvm_guest.lookup_device(converted_pa);
				if (dev != NULL) {
					if (!(run_cpu = handle_device_access(dev, converted_pa, *cpu_run_struct)))
						DEBUG << "Device (" << dev->name() << ") " << (cpu_run_struct->mmio.is_write ? "Write" : "Read") << " Access Failed: " << std::hex << converted_pa;
					break;
				} else {
					DEBUG << "Device Not Found: " << std::hex << converted_pa;
				}
			} else if (cpu_run_struct->mmio.phys_addr >= 0xfffff000 && cpu_run_struct->mmio.phys_addr < 0x100000000) {
				bzero(cpu_run_struct->mmio.data, sizeof(cpu_run_struct->mmio.data));
				break;
			}

			run_cpu = false;
			DEBUG << "EXIT MMIO, pa=" << std::hex << cpu_run_struct->mmio.phys_addr << ", size=" << std::hex << cpu_run_struct->mmio.len;

			break;
		case KVM_EXIT_UNKNOWN:
			run_cpu = false;
			DEBUG << "EXIT UNKNOWN, reason=" << cpu_run_struct->hw.hardware_exit_reason;
			break;
		case KVM_EXIT_INTERNAL_ERROR:
			run_cpu = false;
			DEBUG << "EXIT INTERNAL ERROR, error=" << cpu_run_struct->internal.suberror;
			for (uint32_t i = 0; i < cpu_run_struct->internal.ndata; i++) {
				DEBUG << "DATA: " << std::hex << cpu_run_struct->internal.data[i];
			}
			break;
		case KVM_EXIT_FAIL_ENTRY:
			run_cpu = false;
			DEBUG << "EXIT FAIL ENTRY, error=" << cpu_run_struct->fail_entry.hardware_entry_failure_reason;
			break;
		case KVM_EXIT_SHUTDOWN:
			run_cpu = false;
			DEBUG << "EXIT SHUTDOWN";
			break;
		case KVM_EXIT_HYPERCALL:
			run_cpu = false;
			DEBUG << "EXIT HYPERCALL";
			break;
		default:
			run_cpu = false;
			DEBUG << "UNKNOWN: " << cpu_run_struct->exit_reason;
			break;
		}
	} while (run_cpu);

	dump_regs();

	return true;
}

bool KVMCpu::handle_device_access(devices::Device* device, uint64_t pa, kvm_run& rs)
{
	uint64_t offset = pa & 0xfff;
	//DEBUG << "Handling Device Access: is-write=" << (uint32_t)rs.mmio.is_write << ", offset=" << std::hex << offset << ", len=" << rs.mmio.len;

	if (rs.mmio.is_write) {
		return device->write(offset, rs.mmio.len, *(uint64_t *)&rs.mmio.data[0]);
	} else {
		return device->read(offset, rs.mmio.len, *(uint64_t *)&rs.mmio.data[0]);
	}
}

bool KVMCpu::handle_hypercall(uint64_t data)
{
	KVMGuest& kvm_guest = (KVMGuest &)owner();

	DEBUG << "Hypercall " << data;

	switch(data) {
	case 1:
		if (kvm_guest.stage2_init()) {
			struct kvm_regs regs;
			vmioctl(KVM_GET_REGS, &regs);

			// Entry Point
			regs.rip = 0x100000000 + kvm_guest.engine().entrypoint_offset();

			// Stack + Base Pointer
			regs.rsp = 0x110000000;
			regs.rbp = 0;

			// Startup Arguments
			regs.rdi = kvm_guest.next_avail_phys_page();
			regs.rsi = kvm_guest.guest_entrypoint();

			vmioctl(KVM_SET_REGS, &regs);

			// Cause a TLB invalidation - maybe?
			struct kvm_sregs sregs;
			vmioctl(KVM_GET_SREGS, &sregs);
			vmioctl(KVM_SET_SREGS, &sregs);

			/*struct kvm_guest_debug dbg;
			bzero(&dbg, sizeof(dbg));
			dbg.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP;
			if (vmioctl(KVM_SET_GUEST_DEBUG, &dbg)) {
				ERROR << "Unable to set debug flags";
				return false;
			}*/

			return true;
		} else {
			return false;
		}
	case 2:
		DEBUG << "Abort Requested";
		return false;

	case 3:
		dump_regs();
		return true;

	case 4:
		//kvm_guest.map_page(0x10000, 0x100010000, 0);
		return true;
	}

	return false;
}


void KVMCpu::dump_regs()
{
	struct kvm_regs regs;
	struct kvm_sregs sregs;

	int rc = ioctl(fd, KVM_GET_REGS, &regs);
	if (rc) {
		ERROR << "Unable to retrieve CPU registers";
		return;
	}

	rc = ioctl(fd, KVM_GET_SREGS, &sregs);
	if (rc) {
		ERROR << "Unable to retrieve special CPU registers";
		return;
	}

#define PREG(rg) << #rg " = " << std::hex << regs.rg << " (" << std::dec << regs.rg << ")" << std::endl

	DEBUG << "Registers:" << std::endl

	PREG(rax)
	PREG(rbx)
	PREG(rcx)
	PREG(rdx)
	PREG(rsi)
	PREG(rdi)
	PREG(rsp)
	PREG(rbp)

	PREG(r8)
	PREG(r9)
	PREG(r10)
	PREG(r11)
	PREG(r12)
	PREG(r13)
	PREG(r14)
	PREG(r15)

	PREG(rip)
	PREG(rflags)

#undef PREG
#define PSREG(rg) << #rg ": base=" << std::hex << sregs.rg.base << ", limit=" << std::hex << sregs.rg.limit << ", selector=" << std::hex << sregs.rg.selector << std::endl

	PSREG(cs)
	PSREG(ds)
	PSREG(es)
	PSREG(fs)
	PSREG(gs)
	PSREG(ss)

#undef PSREG
#define PCREG(rg) << #rg ": " << std::hex << sregs.rg << std::endl

	PCREG(cr0)
	PCREG(cr2)
	PCREG(cr3)
	PCREG(cr4)
	PCREG(cr8)

#undef PCREG

	<< "gdt base=" << std::hex << sregs.gdt.base << ", limit=" << std::hex << sregs.gdt.limit << std::endl
	<< "idt base=" << std::hex << sregs.idt.base << ", limit=" << std::hex << sregs.idt.limit << std::endl
	<< "efer=" << std::hex << sregs.efer << std::endl;

	// Instruction Data
	KVMGuest& guest = (KVMGuest&)owner();

	uint8_t *sys_mem = (uint8_t *)guest.sys_mem_rgn->host_buffer;

	if (regs.rip > 0x100000000)
		DEBUG << "DATA @ RIP: " << std::hex << (uint32_t)sys_mem[regs.rip-0xffa00000];
}
