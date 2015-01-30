#include <hypervisor/kvm/cpu.h>
#include <hypervisor/kvm/guest.h>
#include <hypervisor/kvm/kvm.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

using namespace captive::hypervisor::kvm;

KVMCpu::KVMCpu(KVMGuest& owner, const GuestCPUConfiguration& config, int id, int fd) : CPU(owner, config), _initialised(false), _id(id), fd(fd), cpu_run_struct(NULL), cpu_run_struct_size(0)
{

}

KVMCpu::~KVMCpu()
{
	if (initialised()) {
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

bool KVMCpu::run()
{
	int rc;

	if (!initialised()) {
		ERROR << "CPU not initialised";
		return false;
	}

	DEBUG << "Running CPU " << id();
	do {
		rc = vmioctl(KVM_RUN);
		if (rc < 0) {
			ERROR << "Unable to run VCPU: " << LAST_ERROR_TEXT;
			return false;
		}

		switch (cpu_run_struct->exit_reason) {
		case KVM_EXIT_IO:
			DEBUG << "EXIT IO";
			break;
		case KVM_EXIT_MMIO:
			DEBUG << "EXIT MMIO, pa=" << std::hex << cpu_run_struct->mmio.phys_addr << ", size=" << std::hex << cpu_run_struct->mmio.len;
			break;
		case KVM_EXIT_UNKNOWN:
			DEBUG << "EXIT UNKNOWN, reason=" << cpu_run_struct->hw.hardware_exit_reason;
			break;
		case KVM_EXIT_INTERNAL_ERROR:
			DEBUG << "EXIT INTERNAL ERROR, error=" << cpu_run_struct->internal.suberror;
			for (uint32_t i = 0; i < cpu_run_struct->internal.ndata; i++) {
				DEBUG << "DATA: " << std::hex << cpu_run_struct->internal.data[i];
			}
			break;
		case KVM_EXIT_FAIL_ENTRY:
			DEBUG << "EXIT FAIL ENTRY, error=" << cpu_run_struct->fail_entry.hardware_entry_failure_reason;
			break;
		default:
			DEBUG << "UNKNOWN: " << cpu_run_struct->exit_reason;
			break;
		}
	} while (false);

	dump_regs();

	return true;
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
#define PSREG(rg) << #rg ": base=" << std::hex << sregs.rg.base << ", limit=" << std::hex << sregs.rg.limit << std::endl

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
	PCREG(cr8);

#undef PCREG
}
