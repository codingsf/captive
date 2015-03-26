#include <engine/engine.h>
#include <hypervisor/kvm/cpu.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/kvm.h>
#include <jit/jit.h>
#include <verify.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <devices/device.h>
#include <devices/irq/irq-controller.h>
#include <devices/timers/callback-tick-source.h>
#include <sys/eventfd.h>
#include <sys/signal.h>
#include <chrono>

USE_CONTEXT(CPU);

using namespace captive::hypervisor::kvm;

KVMCpu::KVMCpu(KVMGuest& owner, const GuestCPUConfiguration& config, int id, int fd, int irqfd, PerCPUData& per_cpu_data, uint64_t per_cpu_virt_addr)
	: CPU(owner, config, per_cpu_data),
	_initialised(false),
	_id(id),
	fd(fd),
	irqfd(irqfd),
	cpu_run_struct(NULL),
	cpu_run_struct_size(0),
	per_cpu_virt_addr(per_cpu_virt_addr)
{

}

KVMCpu::~KVMCpu()
{
	if (initialised()) {
		munmap(cpu_run_struct, cpu_run_struct_size);
	}

	DEBUG << CONTEXT(CPU) << "Closing KVM VCPU fd";
	close(fd);
}

bool KVMCpu::init()
{
	if (initialised()) {
		ERROR << CONTEXT(CPU) << "KVM CPU already initialised";
		return false;
	}

	if (!CPU::init())
		return false;

	if (!setup_interrupts())
		return false;

	// Attach the CPU IRQ controller
	owner().config().cpu_irq_controller->attach(*this);

	cpu_run_struct_size = ioctl(((KVM &)((KVMGuest &)owner()).owner()).kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if ((int)cpu_run_struct_size == -1) {
		ERROR << CONTEXT(CPU) << "Unable to ascertain size of CPU run structure";
		return false;
	}

	DEBUG << CONTEXT(CPU) << "Mapping CPU run structure size=" << std::hex << cpu_run_struct_size;
	cpu_run_struct = (struct kvm_run *)mmap(NULL, cpu_run_struct_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (cpu_run_struct == MAP_FAILED) {
		ERROR << CONTEXT(CPU) << "Unable to mmap CPU run struct";
		return false;
	}

	_initialised = true;
	return true;
}

void KVMCpu::interrupt(uint32_t code)
{
	per_cpu_data().signal_code = code;

	uint64_t data = 0;
	write(irqfd, &data, sizeof(data));
}

static KVMCpu *signal_cpu;

static void handle_signal(int signo)
{
	if (signo == SIGUSR1) {
		signal_cpu->per_cpu_data().async_action = 2;
	} else if (signo == SIGUSR2) {
		signal_cpu->interrupt(0);
	}
}

static std::chrono::high_resolution_clock::time_point cpu_start_time;

bool KVMCpu::run()
{
	KVMGuest& kvm_guest = (KVMGuest &)owner();

	bool run_cpu = true;
	int rc;

	if (!initialised()) {
		ERROR << CONTEXT(CPU) << "CPU not initialised";
		return false;
	}

	signal_cpu = this;
	signal(SIGUSR1, handle_signal);
	signal(SIGUSR2, handle_signal);

	struct kvm_sregs sregs;
	vmioctl(KVM_GET_SREGS, &sregs);
	sregs.cs.base = 0xf0000;
	vmioctl(KVM_SET_SREGS, &sregs);

	cpu_start_time = std::chrono::high_resolution_clock::now();

	DEBUG << CONTEXT(CPU) << "Running CPU " << id();
	do {
		rc = vmioctl(KVM_RUN);
		if (rc < 0) {
			if (errno == EINTR) continue;

			ERROR << CONTEXT(CPU) << "Unable to run VCPU: " << LAST_ERROR_TEXT;
			return false;
		}

		switch (cpu_run_struct->exit_reason) {
		case KVM_EXIT_DEBUG:
			DEBUG << CONTEXT(CPU) << "DEBUG";
			dump_regs();
			break;

		case KVM_EXIT_IO:
			if (cpu_run_struct->io.port == 0xff) {
				struct kvm_regs regs;
				vmioctl(KVM_GET_REGS, &regs);

				//DEBUG << "Handling hypercall " << std::hex << regs.rax;
				run_cpu = handle_hypercall(regs.rax);
				if (!run_cpu) {
					ERROR << CONTEXT(CPU) << "Unhandled hypercall " << std::hex << regs.rax;
				}
			} else if (cpu_run_struct->io.port == 0xfe) {
				struct kvm_regs regs;
				vmioctl(KVM_GET_REGS, &regs);

				if (regs.rax == 0) {
					kvm_guest.do_guest_printf();
				} else {
					fprintf(stderr, "%c", (char)regs.rax & 0xff);
				}
			} else if (cpu_run_struct->io.port == 0xfd) {
				dump_regs();
			} else {
				run_cpu = false;
				DEBUG << CONTEXT(CPU) << "EXIT IO "
					"port=" << std::hex << cpu_run_struct->io.port << ", "
					"data offset=" << std::hex << cpu_run_struct->io.data_offset << ", "
					"count=" << std::hex << cpu_run_struct->io.count;
			}
			break;
		case KVM_EXIT_MMIO:
			if (cpu_run_struct->mmio.phys_addr >= 0x100000000 && cpu_run_struct->mmio.phys_addr < 0x200000000) {
				uint64_t converted_pa = cpu_run_struct->mmio.phys_addr - 0x100000000;
				devices::Device *dev = kvm_guest.lookup_device(converted_pa);
				if (dev != NULL) {
					if (!(run_cpu = handle_device_access(dev, converted_pa, *cpu_run_struct)))
						DEBUG << CONTEXT(CPU) << "Device (" << dev->name() << ") " << (cpu_run_struct->mmio.is_write ? "Write" : "Read") << " Access Failed: " << std::hex << converted_pa;
					break;
				} else {
					ERROR << CONTEXT(CPU) << "Device Not Found: " << std::hex << converted_pa;
				}
			}

			run_cpu = false;
			ERROR << CONTEXT(CPU) << "EXIT MMIO, pa=" << std::hex << cpu_run_struct->mmio.phys_addr << ", size=" << std::hex << cpu_run_struct->mmio.len;

			break;
		case KVM_EXIT_UNKNOWN:
			run_cpu = false;
			ERROR << CONTEXT(CPU) << "EXIT UNKNOWN, reason=" << cpu_run_struct->hw.hardware_exit_reason;
			break;
		case KVM_EXIT_INTERNAL_ERROR:
			run_cpu = false;
			ERROR << CONTEXT(CPU) << "EXIT INTERNAL ERROR, error=" << cpu_run_struct->internal.suberror;
			for (uint32_t i = 0; i < cpu_run_struct->internal.ndata; i++) {
				ERROR << "DATA: " << std::hex << cpu_run_struct->internal.data[i];
			}
			break;
		case KVM_EXIT_FAIL_ENTRY:
			run_cpu = false;
			ERROR << CONTEXT(CPU) << "EXIT FAIL ENTRY, error=" << cpu_run_struct->fail_entry.hardware_entry_failure_reason;
			break;
		case KVM_EXIT_SHUTDOWN:
			run_cpu = false;
			ERROR << CONTEXT(CPU) << "EXIT SHUTDOWN";
			break;
		case KVM_EXIT_HYPERCALL:
			run_cpu = false;
			ERROR << CONTEXT(CPU) << "EXIT HYPERCALL";
			break;
		case KVM_EXIT_HLT:
			run_cpu = false;
			ERROR << CONTEXT(CPU) << "EXIT HALT";
			break;
		default:
			run_cpu = false;
			ERROR << CONTEXT(CPU) << "UNKNOWN: " << cpu_run_struct->exit_reason;
			break;
		}
	} while (run_cpu && !per_cpu_data().halt);

	dump_regs();

	return true;
}

void KVMCpu::stop()
{
	per_cpu_data().halt = true;
}

bool KVMCpu::handle_device_access(devices::Device* device, uint64_t pa, kvm_run& rs)
{
	uint64_t offset = pa & (device->size() - 1);
	//DEBUG << CONTEXT(CPU) << "Handling Device Access: is-write=" << (uint32_t)rs.mmio.is_write << ", offset=" << std::hex << offset << ", len=" << rs.mmio.len;

	if (rs.mmio.is_write) {
		uint64_t masked_data = *(uint64_t *)&rs.mmio.data[0];

		if (rs.mmio.len < 8) {
			masked_data &= (1ULL << (rs.mmio.len * 8)) - 1;
		}

		return device->write(offset, rs.mmio.len, masked_data);
	} else {
		uint64_t masked_data;
		if (!device->read(offset, rs.mmio.len, masked_data))
			return false;

		if (rs.mmio.len < 8) {
			masked_data &= (1ULL << (rs.mmio.len * 8)) - 1;
		}

		*(uint64_t *)&rs.mmio.data[0] = masked_data;
		return true;
	}
}

bool KVMCpu::handle_hypercall(uint64_t data)
{
	KVMGuest& kvm_guest = (KVMGuest &)owner();

	//DEBUG << CONTEXT(CPU) << "Hypercall " << data;

	switch(data) {
	case 1:
	{
		uint64_t stack;

		if (kvm_guest.stage2_init(stack)) {
			struct kvm_regs regs;
			vmioctl(KVM_GET_REGS, &regs);

			// Entry Point
			regs.rip = kvm_guest.engine().entrypoint();

			// Stack + Base Pointer
			regs.rsp = stack;
			regs.rbp = 0;

			// Startup Arguments
			regs.rdi = per_cpu_virt_addr;

			per_cpu_data().entrypoint = kvm_guest.guest_entrypoint();

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
	}

	case 2:
		DEBUG << CONTEXT(CPU) << "Abort Requested";
		return false;

	case 3:
		DEBUG << CONTEXT(CPU) << "Guest Assert Failure";
		dump_regs();
		return false;

	case 4: {
		std::chrono::high_resolution_clock::time_point then = std::chrono::high_resolution_clock::now();
		std::chrono::seconds dur = std::chrono::duration_cast<std::chrono::seconds>(then - cpu_start_time);

		DEBUG << "Instruction Count: " << per_cpu_data().insns_executed;
		DEBUG << "MIPS: " << ((per_cpu_data().insns_executed * 1e-6) / dur.count());
		return true;
	}

	case 5:
		dump_regs();
		fgetc(stdin);
		return true;

	case 6: {	// COMPILE
		struct kvm_regs regs;
		vmioctl(KVM_GET_REGS, &regs);

		DEBUG << CONTEXT(CPU) << "Compiling Block, incoming offset=" << regs.rdi;
		regs.rax = kvm_guest.jit().block_jit().compile_block(regs.rdi);
		DEBUG << CONTEXT(CPU) << "Compiled Block, outgoing offset=" << regs.rax;

		vmioctl(KVM_SET_REGS, &regs);
		return true;
	}

	case 7: {
		struct kvm_regs regs;
		vmioctl(KVM_GET_REGS, &regs);

		DEBUG << CONTEXT(CPU) << "Releasing Block, incoming offset=" << regs.rdi;
		return true;
	}

	case 8: {	// COMPILE
		DEBUG << CONTEXT(CPU) << "Compiling Region";
		kvm_guest.jit().region_jit().compile_region_async(0, 0, ([](void *data, bool success, uint64_t addr)->void{
			if (success) {
				((KVMCpu *)data)->per_cpu_data().region_addr = addr;
				((KVMCpu *)data)->interrupt(1);
			}
		}), this);

		return true;
	}

	case 9:
		config().verify_tick_source()->do_tick();
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
#define PSREG(rg) << #rg ": base=" << std::hex << sregs.rg.base << ", limit=" << std::hex << sregs.rg.limit << ", selector=" << std::hex << sregs.rg.selector << ", dpl=" << (uint32_t)sregs.rg.dpl << ", avl=" << (uint32_t)sregs.rg.avl << std::endl

	PSREG(cs)
	PSREG(ds)
	PSREG(es)
	PSREG(fs)
	PSREG(gs)
	PSREG(ss)
	PSREG(tr)

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
	<< "apic base=" << std::hex << sregs.apic_base << std::endl
	<< "efer=" << std::hex << sregs.efer << std::endl;
}

bool KVMCpu::setup_interrupts()
{
	struct kvm_lapic_state lapic;
	/*if (!vmioctl(KVM_GET_LAPIC, &lapic)) {
		ERROR << "Unable to retrieve LAPIC state";
		return false;
	}*/

	/*if (!vmioctl(KVM_SET_LAPIC, &lapic)) {
		ERROR << "Unable to update LAPIC state";
		return false;
	}*/

	return true;
}
