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
#include <fcntl.h>
#include <chrono>
#include <sstream>
#include <iomanip>

USE_CONTEXT(CPU);

using namespace captive::hypervisor::kvm;

KVMCpu::KVMCpu(int id, KVMGuest& owner, const GuestCPUConfiguration& config, int fd, PerCPUData *per_cpu_data)
	: CPU(id, owner, config, per_cpu_data),
	_initialised(false),
	fd(fd),
	cpu_run_struct(NULL),
	cpu_run_struct_size(0),
	irq_signal(owner),
	irq_raise(owner),
	irq_rescind(owner),
	irq_ack(owner)
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
	config().irq_controller().attach(*this);

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
	per_cpu_data().async_action = code;
	irq_signal.raise();
}

void KVMCpu::raise_guest_interrupt(uint8_t irq)
{
	irq_raise.raise();
}

void KVMCpu::rescind_guest_interrupt(uint8_t irq)
{
	irq_rescind.raise();
}

void KVMCpu::acknowledge_guest_interrupt(uint8_t irq)
{
	irq_ack.raise();
}

bool KVMCpu::run()
{
	KVMGuest& kvm_guest = (KVMGuest &)owner();

	bool run_cpu = true;
	int rc;

	if (!initialised()) {
		ERROR << CONTEXT(CPU) << "CPU not initialised";
		return false;
	}

	struct kvm_regs regs;
	vmioctl(KVM_GET_REGS, &regs);
	
	regs.rdi = GUEST_SYS_GUEST_DATA_VIRT + (0x100 * (id() + 1));
	regs.rip = kvm_guest.engine().boot_entrypoint();
	regs.rsi = id();
	regs.rflags = 2;
	regs.rsp = GUEST_HEAP_VIRT_BASE + HEAP_SIZE - (0x100000 * (id()));
	
	vmioctl(KVM_SET_REGS, &regs);
	
	struct kvm_sregs sregs;
	vmioctl(KVM_GET_SREGS, &sregs);
	sregs.idt.base = 0;
	sregs.idt.limit = 0;
	
	sregs.gdt.base = GUEST_SYS_GDT_VIRT;
	sregs.gdt.limit = 0x38;
	
	sregs.cr0 &= ~4;
	sregs.cr0 |= 0x80010007;	
	
	sregs.cr3 = GUEST_SYS_INIT_PGT_PHYS;
	sregs.cr4 = 0x6b0;
	
	sregs.efer |= 0x901;
	
	struct kvm_segment cs, ds, tr;
	cs.base = 0;
	cs.limit = 0xffffffff;
	cs.dpl = 0;
	cs.db = 0;
	cs.g = 1;
	cs.s = 1;
	cs.l = 1;
	cs.type = 0xb;
	cs.present = 1;
	cs.selector = 0x08;
	
	ds.base = 0;
	ds.limit = 0xffffffff;
	ds.dpl = 0;
	ds.db = 1;
	ds.g = 1;
	ds.s = 1;
	ds.l = 0;
	ds.type = 0x3;
	ds.present = 1;
	ds.selector = 0x10;
	
	tr.base = 0;
	tr.limit = 0xffffffff;
	tr.dpl = 0;
	tr.db = 1;
	tr.g = 1;
	tr.s = 0;
	tr.l = 0;
	tr.type = 0xb;
	tr.present = 1;
	tr.selector = 0x2b;
	
	sregs.ss = ds;
	sregs.cs = cs;
	sregs.ds = ds;
	sregs.es = ds;
	sregs.fs = ds;
	sregs.gs = ds;
	sregs.tr = tr;
	
	vmioctl(KVM_SET_SREGS, &sregs);

	cpu_start_time = std::chrono::high_resolution_clock::now();

	DEBUG << CONTEXT(CPU) << "Running CPU " << id();
	do {
		rc = vmioctl(KVM_RUN);
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
			}

			ERROR << CONTEXT(CPU) << "Unable to run VCPU: " << LAST_ERROR_TEXT;
			
			run_cpu = false;
			continue;
		}
		
		fprintf(stderr, "************* exit\n");

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
				run_cpu = handle_hypercall(regs.rax, regs.rdi, regs.rsi);
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
			} else if (cpu_run_struct->io.port == 0xf1) {
				struct kvm_regs regs;
				vmioctl(KVM_GET_REGS, &regs);
				instrument_fn_enter(regs.rdi, regs.rsi);
			} else if (cpu_run_struct->io.port == 0xf2) {
				struct kvm_regs regs;
				vmioctl(KVM_GET_REGS, &regs);
				instrument_fn_exit(regs.rdi, regs.rsi);
			} else if (cpu_run_struct->io.port == 0xf0) {
				struct kvm_regs regs;
				vmioctl(KVM_GET_REGS, &regs);

				uint64_t base_addr;
				devices::Device *dev = kvm_guest.lookup_device(regs.rdx, base_addr);
				if (dev != NULL) {
					uint64_t offset = regs.rdx - base_addr;
					if (cpu_run_struct->io.direction == KVM_EXIT_IO_OUT) {
						// Device Write
						dev->write(offset, cpu_run_struct->io.size, *(uint64_t *)((uint64_t)cpu_run_struct + cpu_run_struct->io.data_offset));
					} else {
						// Device Read
						dev->read(offset, cpu_run_struct->io.size, *(uint64_t *)((uint64_t)cpu_run_struct + cpu_run_struct->io.data_offset));
					}
				}
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
				uint64_t converted_pa = cpu_run_struct->mmio.phys_addr & 0xffffffff;
				
				uint64_t base_addr;
				devices::Device *dev = kvm_guest.lookup_device(converted_pa, base_addr);
				
				if (dev != NULL) {
					uint64_t offset = converted_pa - base_addr;
					if (cpu_run_struct->mmio.is_write) {
						run_cpu = dev->write(offset, cpu_run_struct->mmio.len, *(uint64_t *)&cpu_run_struct->mmio.data[0]);
					} else {
						run_cpu = dev->read(offset, cpu_run_struct->mmio.len, *(uint64_t *)&cpu_run_struct->mmio.data[0]);
					}
					
					if (!run_cpu) {
						fprintf(stderr, "device io failed: %s: base=%lx, pa=%lx\n", dev->name().c_str(), base_addr, converted_pa);
					}
					
					/*if (!(run_cpu = handle_device_access(dev, converted_pa, *cpu_run_struct))) {
						DEBUG << CONTEXT(CPU) << "Device (" << dev->name() << ") " << (cpu_run_struct->mmio.is_write ? "Write" : "Read") << " Access Failed: " << std::hex << converted_pa;
					}*/
					
					continue;
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
	interrupt(1);
}

#pragma GCC diagnostic ignored "-Wstrict-aliasing"
bool KVMCpu::handle_device_access(devices::Device* device, uint64_t pa, kvm_run& rs)
{
	uint64_t offset = pa & (device->size() - 1);
	DEBUG << CONTEXT(CPU) << "Handling Device Access: pa=" << std::hex << pa << ", name=" << device->name() << ", is-write=" << (uint32_t)rs.mmio.is_write << ", offset=" << std::hex << offset << ", len=" << rs.mmio.len;

	if (rs.mmio.is_write) {
		return device->write(offset, rs.mmio.len, *(uint64_t *)&rs.mmio.data[0]);
	} else {
		if (!device->read(offset, rs.mmio.len, *(uint64_t *)&rs.mmio.data[0]))
			return false;
		return true;
	}
}
#pragma GCC diagnostic pop

bool KVMCpu::handle_hypercall(uint64_t data, uint64_t arg1, uint64_t arg2)
{
	KVMGuest& kvm_guest = (KVMGuest &)owner();

	DEBUG << CONTEXT(CPU) << "Hypercall " << data;

	switch(data) {
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

		DEBUG << "Instruction Count: " << per_cpu_data().insns_executed << ENABLE;
		DEBUG << "MIPS: " << ((per_cpu_data().insns_executed * 1e-6) / dur.count()) << ENABLE;
		return true;
	}

	case 5:
		dump_regs();
		fgetc(stdin);
		return true;

	case 13: {
		std::stringstream cmd;
		cmd << "addr2line -e arch/arm.arch " << std::hex << arg1;
		system(cmd.str().c_str());
		return true;
	}
	
	case 15: {
		struct kvm_regs regs;
		vmioctl(KVM_GET_REGS, &regs);

		std::stringstream fname;
		fname << "code-" << std::hex << std::setw(8) << std::setfill('0') << regs.rdx << ".bin";
		FILE *f = fopen(fname.str().c_str(), "wb");

		void *ptr = (void *)regs.rdi;
		if (ptr) {
			fwrite(ptr, regs.rsi, 1, f);
		}

		fflush(f);
		fclose(f);
		return true;
	}
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
	if (!irq_signal.attach((id() * 4) + 0)) return false;
	if (!irq_raise.attach((id() * 4) + 1)) return false;
	if (!irq_rescind.attach((id() * 4) + 2)) return false;
	if (!irq_ack.attach((id() * 4) + 3)) return false;
	
	return true;
}

IRQFD::IRQFD(KVMGuest& owner) : owner(owner), fd(-1), gsi(0)
{
	fd = eventfd(0, O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		ERROR << "Unable to create IRQ fd";
		throw 0;
	}
}

IRQFD::~IRQFD()
{
	close(fd);
}

bool IRQFD::attach(int gsi)
{
	assert(fd >= 0);
	DEBUG << CONTEXT(CPU) << "Attaching IRQFD to GSI=" << gsi << ENABLE;

	struct kvm_irqfd irqfd;
	bzero(&irqfd, sizeof(irqfd));

	irqfd.fd = fd;
	irqfd.gsi = gsi;

	if (owner.vmioctl(KVM_IRQFD, &irqfd)) {
		ERROR << "Unable to install IRQ fd";
		return false;
	}
	
	return true;
}

void IRQFD::raise()
{
	uint64_t data = 0;
	write(fd, &data, sizeof(data));
}
