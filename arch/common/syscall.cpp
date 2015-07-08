#include <syscall.h>
#include <printf.h>
#include <cpu.h>
#include <priv.h>

extern "C" {
	uint32_t do_syscall(struct mcontext *mctx, uint64_t syscall_nr, uint64_t arg)
	{
		switch (syscall_nr) {
		case 1: // Flush
		case 2: // Flush ITLB
		case 3: // Flush DTLB
			captive::arch::CPU::get_active_cpu()->mmu().flush();
			return 0;
			
		case 4:	// Flush ITLB Entry
		case 5: // Flush DTLB Entry
			captive::arch::CPU::get_active_cpu()->mmu().invalidate((gva_t)arg);
			return 0;
		}
		return -1;
	}
}
