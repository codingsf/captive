#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>
#include <device.h>
#include <x86/decode.h>

using namespace captive::arch;

static void rewrite_device_access(uint64_t rip, uint8_t *code, const captive::arch::x86::MemoryInstruction& inst)
{
	if (inst.length < 2) return;
	
	bool is_device_read = inst.Dest.type == x86::Operand::TYPE_REGISTER;
	
	if (code[0] == 0x67) {
		code[0] = is_device_read ? 0xc4 : 0xc5;
	} else if (code[1] == 0x67) {
		code[1] = code[0];
		code[0] = is_device_read ? 0xc4 : 0xc5;
	} else {
		return;
	}
}

static void handle_device_fault(captive::arch::CPU *core, struct mcontext *mctx, gpa_t dev_addr)
{
	//printf("fault: device fault rip=%lx\n", mctx->rip);

	/*printf("code: ");
	for (int i = 0; i < 8; i++) {
		printf("%02x ", ((uint8_t *)mctx->rip)[i]);
	}
	printf("\n");*/
	
	captive::arch::x86::MemoryInstruction inst;
	if (!x86::decode_memory_instruction((const uint8_t *)mctx->rip, inst)) {
		printf("code: ");
		for (int i = 0; i < 8; i++) {
			printf("%02x ", ((uint8_t *)mctx->rip)[i]);
		}
		printf("\n");
		
		fatal("unable to decode memory instruction\n");
	}

	if (inst.Source.type == x86::Operand::TYPE_REGISTER && inst.Dest.type == x86::Operand::TYPE_MEMORY) {
		switch (inst.Source.reg) {
		case x86::Operand::R_EAX: mmio_device_write(dev_addr, 4, mctx->rax); break;
		case x86::Operand::R_EBX: mmio_device_write(dev_addr, 4, mctx->rbx); break;
		case x86::Operand::R_ECX: mmio_device_write(dev_addr, 4, mctx->rcx); break;
		case x86::Operand::R_EDX: mmio_device_write(dev_addr, 4, mctx->rdx); break;
		case x86::Operand::R_ESI: mmio_device_write(dev_addr, 4, mctx->rsi); break;
		case x86::Operand::R_EDI: mmio_device_write(dev_addr, 4, mctx->rdi); break;

		case x86::Operand::R_AX: mmio_device_write(dev_addr, 2, mctx->rax); break;
		case x86::Operand::R_BX: mmio_device_write(dev_addr, 2, mctx->rbx); break;
		case x86::Operand::R_CX: mmio_device_write(dev_addr, 2, mctx->rcx); break;
		case x86::Operand::R_DX: mmio_device_write(dev_addr, 2, mctx->rdx); break;
		case x86::Operand::R_SI: mmio_device_write(dev_addr, 2, mctx->rsi); break;
		case x86::Operand::R_DI: mmio_device_write(dev_addr, 2, mctx->rdi); break;

		case x86::Operand::R_AL: mmio_device_write(dev_addr, 1, mctx->rax); break;
		case x86::Operand::R_BL: mmio_device_write(dev_addr, 1, mctx->rbx); break;
		case x86::Operand::R_CL: mmio_device_write(dev_addr, 1, mctx->rcx); break;
		case x86::Operand::R_DL: mmio_device_write(dev_addr, 1, mctx->rdx); break;
		case x86::Operand::R_SIL: mmio_device_write(dev_addr, 1, mctx->rsi); break;
		case x86::Operand::R_DIL: mmio_device_write(dev_addr, 1, mctx->rdi); break;
		
		case x86::Operand::R_R8B: mmio_device_write(dev_addr, 1, mctx->r8); break;
		case x86::Operand::R_R9B: mmio_device_write(dev_addr, 1, mctx->r9); break;
		case x86::Operand::R_R10B: mmio_device_write(dev_addr, 1, mctx->r10); break;
		case x86::Operand::R_R11B: mmio_device_write(dev_addr, 1, mctx->r11); break;
		case x86::Operand::R_R12B: mmio_device_write(dev_addr, 1, mctx->r12); break;
		case x86::Operand::R_R13B: mmio_device_write(dev_addr, 1, mctx->r13); break;
		case x86::Operand::R_R14B: mmio_device_write(dev_addr, 1, mctx->r14); break;
		case x86::Operand::R_R15B: mmio_device_write(dev_addr, 1, mctx->r15); break;

		default: fatal("unhandled source register %s\n", x86::x86_register_names[inst.Source.reg]);
		}
	} else if (inst.Source.type == x86::Operand::TYPE_MEMORY && inst.Dest.type == x86::Operand::TYPE_REGISTER) {
		uint64_t value;
		mmio_device_read(dev_addr, inst.data_size, value);
		
		switch (inst.Dest.reg) {
		case x86::Operand::R_EAX: mctx->rax = (uint32_t)value; break;
		case x86::Operand::R_EBX: mctx->rbx = (uint32_t)value; break;
		case x86::Operand::R_ECX: mctx->rcx = (uint32_t)value; break;
		case x86::Operand::R_EDX: mctx->rdx = (uint32_t)value; break;
		case x86::Operand::R_ESI: mctx->rsi = (uint32_t)value; break;
		case x86::Operand::R_EDI: mctx->rdi = (uint32_t)value; break;
		
		case x86::Operand::R_AX: mctx->rax = (mctx->rax & ~0xffffULL) | (uint16_t)value; break;
		case x86::Operand::R_BX: mctx->rbx = (mctx->rbx & ~0xffffULL) | (uint16_t)value; break;
		case x86::Operand::R_CX: mctx->rcx = (mctx->rcx & ~0xffffULL) | (uint16_t)value; break;
		case x86::Operand::R_DX: mctx->rdx = (mctx->rdx & ~0xffffULL) | (uint16_t)value; break;
		case x86::Operand::R_SI: mctx->rsi = (mctx->rsi & ~0xffffULL) | (uint16_t)value; break;
		case x86::Operand::R_DI: mctx->rdi = (mctx->rdi & ~0xffffULL) | (uint16_t)value; break;

		case x86::Operand::R_AL: mctx->rax = (mctx->rax & ~0xffULL) | (uint8_t)value; break;
		case x86::Operand::R_BL: mctx->rbx = (mctx->rbx & ~0xffULL) | (uint8_t)value; break;
		case x86::Operand::R_CL: mctx->rcx = (mctx->rcx & ~0xffULL) | (uint8_t)value; break;
		case x86::Operand::R_DL: mctx->rdx = (mctx->rdx & ~0xffULL) | (uint8_t)value; break;
		
		default: fatal("unhandled dest register %s\n", x86::x86_register_names[inst.Dest.reg]);
		}
	} else {
		fatal("illegal combination of operands for memory instruction\n");
	}

	if ((int64_t)mctx->rip > 0) {
		//rewrite_device_access(mctx->rip, (uint8_t *)mctx->rip, inst);
	}
	
	// Skip over the instruction
	mctx->rip += inst.length;
}

#define PF_PRESENT		(1 << 0)
#define PF_WRITE		(1 << 1)
#define PF_USER_MODE	(1 << 2)

static const char *info_modes[] = {
	"user",
	"kernel"
};

static const char *info_reasons[] = {
	"page invalid",
	"permissions failure"
};

static const char *info_types[] = {
	"read",
	"write",
	"fetch"
};

extern "C" int handle_pagefault(struct mcontext *mctx, uint64_t va)
{
	uint64_t code = mctx->extra;
	uint64_t rip = mctx->rip;
	
	// If the virtual address is in the lower 4GB, then is is a guest
	// instruction (or decode) taking a memory fault.
	if (va < 0x200000000) {	// XXX HACK HACK HACK
		bool emulate_user = false;
		if (va >= GPM_EMULATED_VIRT_START) {
			emulate_user = true;
		}
		
		// Obtain the core that is currently active.
		captive::arch::CPU *core = captive::arch::CPU::get_active_cpu();

		if (core) {
			MMU::resolution_context rc(va);
			
			bool user_mode = false;
			
			// Prepare an access_info structure to describe the memory access
			// to the MMU.
			if (emulate_user) {
				rc.emulate_user = true;
				user_mode = true;
			} else {
				rc.emulate_user = false;
				user_mode = !!(code & PF_USER_MODE);
			}

			if ((uint32_t)va == core->read_pc() && !(code & PF_WRITE)) {
				// Detect a fetch
				rc.requested_permissions = user_mode ? MMU::USER_FETCH : MMU::KERNEL_FETCH;
			} else {
				// Detect a READ/WRITE
				if (code & PF_WRITE) {
					rc.requested_permissions = user_mode ? MMU::USER_WRITE : MMU::KERNEL_WRITE;
				} else {
					rc.requested_permissions = user_mode ? MMU::USER_READ : MMU::KERNEL_READ;
				}
			}

			// Get the core's MMU to handle the fault.
			if (core->mmu().handle_fault(rc)) {
				// If we got this far, then the fault was handled by the core's logic.
				//printf("mmu: handled page-fault: va=%lx, code=%x, pc=%x, fault=%d, mode=%s, type=%s, reason=%s\n", va, code, core->read_pc(), fault, info_modes[info.mode], info_types[info.type], info_reasons[info.reason]);

				if (rc.fault == MMU::DEVICE_FAULT) {
					handle_device_fault(core, mctx, rc.pa);
					return 0;
				} else if (rc.fault == MMU::SMC_FAULT) {
					return 2;
				} else if (rc.fault == MMU::NONE) {
					return 0;
				} else {
					// Invoke the target platform behaviour for the memory fault
					// Return TRUE if we need to return to the safe-point, i.e. to do a side
					// exit from the currently executing guest instruction.
					core->handle_mmu_fault(rc.fault);
					return 1;
				}
			} else {
				// If the core couldn't handle the fault, then we've got a serious problem.
				fatal("unhandled page-fault: va=%lx, code=%x, pc=%x\n", va, code, core->read_pc());
			}
		} else {
			// We can't handle this page fault if we haven't got an active core.
			fatal("unhandled page-fault: va=%lx, code=%x, (no cpu)\n", va, code);
		}
	} else {
		// This page-fault happened elsewhere - we can't do anything about it.
		printf("fatal: internal page-fault: rip=%lx va=%lx, code=%x\n", rip, va, code);

		printf("  type:   %s\n", (code & PF_WRITE) ? "write" : "read");
		printf("  mode:   %s\n", (code & PF_USER_MODE) ? "user" : "kernel");
		printf("  reason: %s\n", (code & PF_PRESENT) ? "permission" : "not present");
		
		dump_mcontext(mctx);
		dump_stack();

		abort();
	}

	unreachable();
}
