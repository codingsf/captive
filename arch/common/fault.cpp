#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>
#include <x86/decode.h>

using namespace captive::arch;

static void handle_device_fault(captive::arch::CPU *core, struct mcontext *mctx, gpa_t dev_addr)
{
	/*printf("fault: device fault rip=%lx\n", mctx->rip);

	printf("code: ");
	for (int i = 0; i < 8; i++) {
		printf("%02x ", ((uint8_t *)mctx->rip)[i]);
	}
	printf("\n");*/

	core->cpu_data().device_address = dev_addr;

	captive::arch::x86::MemoryInstruction inst;
	assert(decode_memory_instruction((const uint8_t *)mctx->rip, inst));

	if (inst.Source.type == x86::Operand::TYPE_REGISTER && inst.Dest.type == x86::Operand::TYPE_MEMORY) {
		switch (inst.Source.reg) {
		case x86::Operand::R_EAX: asm volatile("outl %0, $0xf0\n" :: "a"((uint32_t)mctx->rax)); break;
		case x86::Operand::R_EBX: asm volatile("outl %0, $0xf0\n" :: "a"((uint32_t)mctx->rbx)); break;
		case x86::Operand::R_ECX: asm volatile("outl %0, $0xf0\n" :: "a"((uint32_t)mctx->rcx)); break;
		case x86::Operand::R_EDX: asm volatile("outl %0, $0xf0\n" :: "a"((uint32_t)mctx->rdx)); break;
		case x86::Operand::R_ESI: asm volatile("outl %0, $0xf0\n" :: "a"((uint32_t)mctx->rsi)); break;
		case x86::Operand::R_EDI: asm volatile("outl %0, $0xf0\n" :: "a"((uint32_t)mctx->rdi)); break;

		case x86::Operand::R_AX: asm volatile("outw %0, $0xf0\n" :: "a"((uint16_t)mctx->rax)); break;
		case x86::Operand::R_BX: asm volatile("outw %0, $0xf0\n" :: "a"((uint16_t)mctx->rbx)); break;
		case x86::Operand::R_CX: asm volatile("outw %0, $0xf0\n" :: "a"((uint16_t)mctx->rcx)); break;
		case x86::Operand::R_DX: asm volatile("outw %0, $0xf0\n" :: "a"((uint16_t)mctx->rdx)); break;
		case x86::Operand::R_SI: asm volatile("outw %0, $0xf0\n" :: "a"((uint16_t)mctx->rsi)); break;
		case x86::Operand::R_DI: asm volatile("outw %0, $0xf0\n" :: "a"((uint16_t)mctx->rdi)); break;

		case x86::Operand::R_AL: asm volatile("outb %0, $0xf0\n" :: "a"((uint8_t)mctx->rax)); break;
		case x86::Operand::R_BL: asm volatile("outb %0, $0xf0\n" :: "a"((uint8_t)mctx->rbx)); break;
		case x86::Operand::R_CL: asm volatile("outb %0, $0xf0\n" :: "a"((uint8_t)mctx->rcx)); break;
		case x86::Operand::R_DL: asm volatile("outb %0, $0xf0\n" :: "a"((uint8_t)mctx->rdx)); break;

		default: abort();
		}
	} else if (inst.Source.type == x86::Operand::TYPE_MEMORY && inst.Dest.type == x86::Operand::TYPE_REGISTER) {
		uint32_t value;
		asm volatile("inl $0xf0, %0\n" : "=a"(value));

		switch (inst.Dest.reg) {
		case x86::Operand::R_EAX: mctx->rax = value; break;
		case x86::Operand::R_EBX: mctx->rbx = value; break;
		case x86::Operand::R_ECX: mctx->rcx = value; break;
		case x86::Operand::R_EDX: mctx->rdx = value; break;
		case x86::Operand::R_ESI: mctx->rsi = value; break;
		case x86::Operand::R_EDI: mctx->rdi = value; break;
		default: abort();
		}
	} else {
		abort();
	}

	// Skip over the instruction
	mctx->rip += inst.length;
}

#define PF_PRESENT	(1 << 0)
#define PF_WRITE	(1 << 1)
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
	if (va < 0x100000000) {
		// Obtain the core that is currently active.
		captive::arch::CPU *core = captive::arch::CPU::get_active_cpu();

		if (core) {
			MMU::resolution_fault fault;
			MMU::access_info info;

			// Prepare an access_info structure to describe the memory access
			// to the MMU.
			info.mode = (code & PF_USER_MODE) ? MMU::ACCESS_USER : MMU::ACCESS_KERNEL;
			//info.mode = (core->kernel_mode() && !core->emulating_user_mode()) ? MMU::ACCESS_KERNEL : MMU::ACCESS_USER;

			if (code & PF_PRESENT) {
				// Detect the fault as being because a permissions check failed
				info.reason = MMU::REASON_PERMISSIONS_FAIL;
			} else {
				// Detect the fault as being because the accessed page is invalid
				info.reason = MMU::REASON_PAGE_INVALID;
			}

			if (va == core->read_pc() && !(code & PF_WRITE)) {
				// Detect a fetch
				info.type = MMU::ACCESS_FETCH;
			} else {
				// Detect a READ/WRITE
				if (code & PF_WRITE) {
					info.type = MMU::ACCESS_WRITE;
				} else {
					info.type = MMU::ACCESS_READ;
				}
			}

			// Get the core's MMU to handle the fault.
			gpa_t out_pa;
			if (core->mmu().handle_fault((gva_t)va, out_pa, info, fault)) {
				// If we got this far, then the fault was handled by the core's logic.
				//printf("mmu: handled page-fault: va=%lx, code=%x, pc=%x, fault=%d, mode=%s, type=%s, reason=%s\n", va, code, core->read_pc(), fault, info_modes[info.mode], info_types[info.type], info_reasons[info.reason]);

				if (fault == MMU::DEVICE_FAULT) {
					handle_device_fault(core, mctx, out_pa);
					return 0;
				}

				// Return TRUE if we need to return to the safe-point, i.e. to do a side
				// exit from the currently executing guest instruction.
				return (int)fault;
			} else {
				// If the core couldn't handle the fault, then we've got a serious problem.
				printf("panic: unhandled page-fault: va=%lx, code=%x, pc=%x\n", va, code, core->read_pc());
				abort();
			}
		} else {
			// We can't handle this page fault if we haven't got an active core.
			printf("panic: unhandled page-fault: va=%lx, code=%x, (no cpu)\n", va, code);
			abort();
		}
	} else {
		// This page-fault happened elsewhere - we can't do anything about it.
		printf("panic: internal page-fault: rip=%lx va=%lx, code=%x\n", rip, va, code);

		printf("  type:   %s\n", (code & PF_WRITE) ? "write" : "read");
		printf("  mode:   %s\n", (code & PF_USER_MODE) ? "user" : "kernel");
		printf("  reason: %s\n", (code & PF_PRESENT) ? "permission" : "not present");

		abort();
	}

	// We can't ever get here - all other paths have an abort in them.
	return 0;
}
