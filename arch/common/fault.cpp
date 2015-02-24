#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>

extern "C" int handle_pagefault(uint64_t va, uint64_t code)
{
	// If the virtual address is in the lower 4GB, then is is a guest
	// instruction (or decode) taking a memory fault.
	if (va < 0x100000000) {
		// Obtain the core that is currently active.
		captive::arch::CPU *core = captive::arch::active_cpu;

		if (core) {
			captive::arch::MMU::resolution_fault fault;

			// Get the core's MMU to handle the fault.
			if (core->mmu().handle_fault((captive::arch::va_t)va, fault)) {
				// If we got this far, then the fault was handled by the core's logic.

				// Return TRUE if we need to return to the safe-point, i.e. to do a side
				// exit from the currently executing guest instruction.
				return fault == captive::arch::MMU::NONE ? 0 : 1;
			} else {
				// If the core couldn't handle the fault, then we've got a serious problem.
				printf("panic: unhandled page-fault: va=%x, code=%x, pc=%x\n", va, code, core->read_pc());
				abort();
			}
		} else {
			// We can't handle this page fault if we haven't got an active core.
			printf("panic: unhandled page-fault: va=%x, code=%x, (no cpu)\n", va, code);
			abort();
		}
	} else {
		// This page-fault happened elsewhere - we can't do anything about it.
		printf("panic: internal page-fault: va=%x, code=%x\n", va, code);
		abort();
	}

	// We can't ever get here - all other paths have an abort in them.
	return 1;
}
