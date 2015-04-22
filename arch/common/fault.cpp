#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>
#include <x86/decode.h>

using namespace captive::arch;

static inline bool is_prefix(uint8_t bc)
{
	return bc == 0x67 || bc == 0x66;
}

static void handle_device_fault(struct mcontext *mctx, gpa_t dev_addr)
{
	printf("fault: device fault rip=%lx\n", mctx->rip);

	printf("code: ");
	for (int i = 0; i < 8; i++) {
		printf("%02x ", ((uint8_t *)mctx->rip)[i]);
	}
	printf("\n");

	captive::arch::x86::MemoryInstruction inst;
	assert(decode_memory_instruction((const uint8_t *)mctx->rip, inst));

	if (inst.Source.type == x86::Operand::TYPE_REGISTER && inst.Dest.type == x86::Operand::TYPE_MEMORY) {
		uint32_t value;

		switch (inst.Source.reg) {
		case x86::Operand::R_ESI: value = mctx->rsi; break;
		default: abort();
		}

		printf("device write: addr=%x, value=%u\n", dev_addr, value);
		abort();
	} else if (inst.Dest.type == x86::Operand::TYPE_REGISTER) {
		printf("device read: src=%d, dst=%d\n", inst.Source.immed_val, inst.Dest.immed_val);
	} else {
		abort();
	}

	// Skip over the instruction
	mctx->rip += inst.length;
}

#define PF_PRESENT	(1 << 0)
#define PF_WRITE	(1 << 1)
#define PF_USER_MODE	(1 << 2)

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
			//info.mode = (code & PF_USER_MODE) ? MMU::ACCESS_USER : MMU::ACCESS_KERNEL;
			info.mode = core->kernel_mode() ? MMU::ACCESS_KERNEL : MMU::ACCESS_USER;

			if (code & PF_PRESENT) {
				// Detect the fault as being because the accessed page is invalid
				info.reason = MMU::REASON_PAGE_INVALID;
			} else {
				// Detect the fault as being because a permissions check failed
				info.reason = MMU::REASON_PERMISSIONS_FAIL;
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
				//printf("mmu: handled page-fault: va=%lx, code=%x, pc=%x, fault=%d, mode=%d\n", va, code, core->read_pc(), fault, info.mode);

				if (fault == MMU::DEVICE_FAULT) {
					handle_device_fault(mctx, out_pa);
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
		assert(false);
	}

	// We can't ever get here - all other paths have an abort in them.
	return 0;
}
