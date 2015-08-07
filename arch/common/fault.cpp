#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>
#include <x86/decode.h>

using namespace captive::arch;

static inline void __out32(uint32_t address, uint32_t value)
{
	asm("outl %0, $0xf0\n" :: "a"(value), "d"(address));
}

static inline void __out16(uint32_t address, uint16_t value)
{
	asm("outw %0, $0xf0\n" :: "a"(value), "d"(address));
}

static inline void __out8(uint32_t address, uint8_t value)
{
	asm("outb %0, $0xf0\n" :: "a"(value), "d"(address));
}

static inline uint32_t __in32(uint32_t address)
{
	uint32_t value;
	asm("inl $0xf0, %0\n" : "=a"(value) : "d"(address));
	return value;
}

static inline uint16_t __in16(uint32_t address)
{
	uint32_t value;
	asm("inl $0xf0, %0\n" : "=a"(value) : "d"(address));
	return (uint16_t)value;
}

static inline uint8_t __in8(uint32_t address)
{
	uint32_t value;
	asm("inl $0xf0, %0\n" : "=a"(value) : "d"(address));
	return (uint8_t)value;
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
	if (!decode_memory_instruction((const uint8_t *)mctx->rip, inst))
		fatal("unable to decode memory instruction\n");

	if (inst.Source.type == x86::Operand::TYPE_REGISTER && inst.Dest.type == x86::Operand::TYPE_MEMORY) {
		switch (inst.Source.reg) {
		case x86::Operand::R_EAX: __out32(dev_addr, mctx->rax); break;
		case x86::Operand::R_EBX: __out32(dev_addr, mctx->rbx); break;
		case x86::Operand::R_ECX: __out32(dev_addr, mctx->rcx); break;
		case x86::Operand::R_EDX: __out32(dev_addr, mctx->rdx); break;
		case x86::Operand::R_ESI: __out32(dev_addr, mctx->rsi); break;
		case x86::Operand::R_EDI: __out32(dev_addr, mctx->rdi); break;

		case x86::Operand::R_AX: __out16(dev_addr, mctx->rax); break;
		case x86::Operand::R_BX: __out16(dev_addr, mctx->rbx); break;
		case x86::Operand::R_CX: __out16(dev_addr, mctx->rcx); break;
		case x86::Operand::R_DX: __out16(dev_addr, mctx->rdx); break;
		case x86::Operand::R_SI: __out16(dev_addr, mctx->rsi); break;
		case x86::Operand::R_DI: __out16(dev_addr, mctx->rdi); break;

		case x86::Operand::R_AL: __out8(dev_addr, mctx->rax); break;
		case x86::Operand::R_BL: __out8(dev_addr, mctx->rbx); break;
		case x86::Operand::R_CL: __out8(dev_addr, mctx->rcx); break;
		case x86::Operand::R_DL: __out8(dev_addr, mctx->rdx); break;
		case x86::Operand::R_SIL: __out8(dev_addr, mctx->rsi); break;
		case x86::Operand::R_DIL: __out8(dev_addr, mctx->rdi); break;

		default: fatal("unhandled source register %s\n", x86::x86_register_names[inst.Source.reg]);
		}
	} else if (inst.Source.type == x86::Operand::TYPE_MEMORY && inst.Dest.type == x86::Operand::TYPE_REGISTER) {
		uint32_t value = __in32(dev_addr);
		
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
		
		default: fatal("unhandled dest register %s\n", x86::x86_register_names[inst.Dest.reg]);
		}
	} else {
		fatal("illegal combination of operands for memory instruction\n");
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

		abort();
	}

	unreachable();
}
