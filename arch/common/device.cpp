#include <device.h>
#include <shmem.h>
#include <cpu.h>
#include <x86/decode.h>

using namespace captive::arch;

Device::Device(Environment& env) : _env(env)
{

}

Device::~Device()
{

}

CoreDevice::CoreDevice(Environment& env) : Device(env)
{

}

CoreDevice::~CoreDevice()
{

}

void captive::arch::mmio_device_read(gpa_t pa, uint8_t size, uint64_t& value)
{
	captive::PerGuestData *pgd = CPU::get_active_cpu_data()->guest_data;
	
	pgd->fast_device_address = pa;
	pgd->fast_device_size = size;
	
	pgd->fast_device_operation = FAST_DEV_OP_READ;
	
	captive::lock::barrier_wait_nopause(&pgd->fd_hypervisor_barrier, FAST_DEV_GUEST_TID);
	captive::lock::barrier_wait_nopause(&pgd->fd_guest_barrier, FAST_DEV_GUEST_TID);
	
	value = pgd->fast_device_value;
}

void captive::arch::mmio_device_write(gpa_t pa, uint8_t size, uint64_t value)
{
	captive::PerGuestData *pgd = CPU::get_active_cpu_data()->guest_data;
	
	pgd->fast_device_address = pa;
	pgd->fast_device_size = size;
	pgd->fast_device_value = value;
	
	pgd->fast_device_operation = FAST_DEV_OP_WRITE;
	
	captive::lock::barrier_wait_nopause(&pgd->fd_hypervisor_barrier, FAST_DEV_GUEST_TID);
	captive::lock::barrier_wait_nopause(&pgd->fd_guest_barrier, FAST_DEV_GUEST_TID);
}

int captive::arch::handle_fast_device_read(struct mcontext *mctx)
{
	captive::arch::x86::MemoryInstruction inst;
	x86::decode_memory_instruction((const uint8_t *)mctx->rip, inst);
	mctx->rip += inst.length;
	
	assert(inst.Source.type == x86::Operand::TYPE_MEMORY);

	uint32_t va = inst.Source.mem.displacement;
	switch (inst.Source.mem.base_reg_idx) {
	case x86::Operand::R_EAX: va += (uint32_t)mctx->rax; break;
	case x86::Operand::R_EBX: va += (uint32_t)mctx->rbx; break;
	case x86::Operand::R_ECX: va += (uint32_t)mctx->rcx; break;
	case x86::Operand::R_EDX: va += (uint32_t)mctx->rdx; break;
	default: fatal("unhandled base register %s for device read\n", x86::x86_register_names[inst.Source.mem.base_reg_idx]);
	}

	MMU::resolution_context rc(va);
	rc.requested_permissions = CPU::get_active_cpu()->kernel_mode() ? MMU::KERNEL_READ : MMU::USER_READ;

	CPU::get_active_cpu()->mmu().resolve_gpa(rc, true);
	if (rc.fault) {
		printf("read @ %p va=%x fault=%d\n", mctx->rip - 2, va, rc.fault);
		return (int)rc.fault;
	}

	uint64_t value;
	mmio_device_read(rc.pa, inst.data_size, value);
	
	//printf("F read  @ %016lx addr=%08x val=%08x\n", mctx->rip - 2, pa, value);

	switch (inst.Dest.reg) {
	case x86::Operand::R_EAX: mctx->rax = value; break;
	case x86::Operand::R_EBX: mctx->rbx = value; break;
	case x86::Operand::R_ECX: mctx->rcx = value; break;
	case x86::Operand::R_EDX: mctx->rdx = value; break;
	case x86::Operand::R_ESI: mctx->rsi = value; break;
	case x86::Operand::R_EDI: mctx->rdi = value; break;

	case x86::Operand::R_AX: mctx->rax = (mctx->rax & ~0xffffULL) | (value & 0xffff); break;
	case x86::Operand::R_BX: mctx->rbx = (mctx->rbx & ~0xffffULL) | (value & 0xffff); break;
	case x86::Operand::R_CX: mctx->rcx = (mctx->rcx & ~0xffffULL) | (value & 0xffff); break;
	case x86::Operand::R_DX: mctx->rdx = (mctx->rdx & ~0xffffULL) | (value & 0xffff); break;
	case x86::Operand::R_SI: mctx->rsi = (mctx->rsi & ~0xffffULL) | (value & 0xffff); break;
	case x86::Operand::R_DI: mctx->rdi = (mctx->rdi & ~0xffffULL) | (value & 0xffff); break;

	case x86::Operand::R_AL: mctx->rax = (mctx->rax & ~0xffULL) | (value & 0xff); break;
	case x86::Operand::R_BL: mctx->rbx = (mctx->rbx & ~0xffULL) | (value & 0xff); break;
	case x86::Operand::R_CL: mctx->rcx = (mctx->rcx & ~0xffULL) | (value & 0xff); break;
	case x86::Operand::R_DL: mctx->rdx = (mctx->rdx & ~0xffULL) | (value & 0xff); break;

	default: fatal("unhandled dest register %s for device read\n", x86::x86_register_names[inst.Dest.reg]);
	}

	return 0;
}

int captive::arch::handle_fast_device_write(struct mcontext *mctx)
{
	captive::arch::x86::MemoryInstruction inst;
	x86::decode_memory_instruction((const uint8_t *)mctx->rip, inst);
	mctx->rip += inst.length;

	assert(inst.Dest.type == x86::Operand::TYPE_MEMORY);

	uint32_t va = inst.Dest.mem.displacement;
	switch (inst.Dest.mem.base_reg_idx) {
	case x86::Operand::R_EAX: va += (uint32_t)mctx->rax; break;
	case x86::Operand::R_EBX: va += (uint32_t)mctx->rbx; break;
	case x86::Operand::R_ECX: va += (uint32_t)mctx->rcx; break;
	case x86::Operand::R_EDX: va += (uint32_t)mctx->rdx; break;
	default: fatal("unhandled base register %s for device write\n", x86::x86_register_names[inst.Dest.mem.base_reg_idx]);
	}

	MMU::resolution_context rc(va);
	rc.requested_permissions = CPU::get_active_cpu()->kernel_mode() ? MMU::KERNEL_WRITE : MMU::USER_WRITE;

	CPU::get_active_cpu()->mmu().resolve_gpa(rc, true);
	if (rc.fault) {
		printf("write @ %p va=%x fault=%d\n", mctx->rip - 2, va, rc.fault);
		return (int)rc.fault;
	}

	switch (inst.Source.reg) {
	case x86::Operand::R_EAX: mmio_device_write(rc.pa, 4, mctx->rax); break;
	case x86::Operand::R_EBX: mmio_device_write(rc.pa, 4, mctx->rbx); break;
	case x86::Operand::R_ECX: mmio_device_write(rc.pa, 4, mctx->rcx); break;
	case x86::Operand::R_EDX: mmio_device_write(rc.pa, 4, mctx->rdx); break;
	case x86::Operand::R_ESI: mmio_device_write(rc.pa, 4, mctx->rsi); break;
	case x86::Operand::R_EDI: mmio_device_write(rc.pa, 4, mctx->rdi); break;

	case x86::Operand::R_AX: mmio_device_write(rc.pa, 2, mctx->rax); break;
	case x86::Operand::R_BX: mmio_device_write(rc.pa, 2, mctx->rbx); break;
	case x86::Operand::R_CX: mmio_device_write(rc.pa, 2, mctx->rcx); break;
	case x86::Operand::R_DX: mmio_device_write(rc.pa, 2, mctx->rdx); break;
	case x86::Operand::R_SI: mmio_device_write(rc.pa, 2, mctx->rsi); break;
	case x86::Operand::R_DI: mmio_device_write(rc.pa, 2, mctx->rdi); break;

	case x86::Operand::R_AL: mmio_device_write(rc.pa, 1, mctx->rax); break;
	case x86::Operand::R_BL: mmio_device_write(rc.pa, 1, mctx->rbx); break;
	case x86::Operand::R_CL: mmio_device_write(rc.pa, 1, mctx->rcx); break;
	case x86::Operand::R_DL: mmio_device_write(rc.pa, 1, mctx->rdx); break;
	case x86::Operand::R_SIL: mmio_device_write(rc.pa, 1, mctx->rsi); break;
	case x86::Operand::R_DIL: mmio_device_write(rc.pa, 1, mctx->rdi); break;

	default: fatal("unhandled source register %s for device write\n", x86::x86_register_names[inst.Source.reg]);
	}

	return 0;
}
