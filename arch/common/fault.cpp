#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>

using namespace captive::arch;

extern "C" int handle_pagefault(uint64_t va, uint64_t code, uint64_t rip)
{
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
			info.mode = core->kernel_mode() ? MMU::ACCESS_KERNEL : MMU::ACCESS_USER;

			if (va == core->read_pc()) {
				// Detect a fetch
				info.type = MMU::ACCESS_FETCH;
			} else {
				// Detect a READ/WRITE
				if (code & 2) {
					info.type = MMU::ACCESS_WRITE;
				} else {
					info.type = MMU::ACCESS_READ;
				}
			}

			// Get the core's MMU to handle the fault.
			if (core->mmu().handle_fault((gva_t)va, info, fault)) {
				// If we got this far, then the fault was handled by the core's logic.

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
