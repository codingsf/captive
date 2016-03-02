#include <syscall.h>
#include <printf.h>
#include <cpu.h>
#include <priv.h>

//#define DEBUG_SYSCALLS

extern "C" {
	uint32_t do_syscall(struct mcontext *mctx, uint64_t syscall_nr, uint64_t arg)
	{
		switch (syscall_nr) {
		case 1: // Flush
#ifdef DEBUG_SYSCALLS
			printf("syscall: flush\n");
#endif
			captive::arch::CPU::get_active_cpu()->mmu().invalidate_virtual_mappings();
			break;
			
		case 2: // Flush ITLB
#ifdef DEBUG_SYSCALLS
			printf("syscall: flush itlb\n");
#endif
			captive::arch::CPU::get_active_cpu()->mmu().invalidate_virtual_mappings();
			break;

		case 3: // Flush DTLB
#ifdef DEBUG_SYSCALLS
			printf("syscall: flush dtlb\n");
#endif
			captive::arch::CPU::get_active_cpu()->mmu().invalidate_virtual_mappings();
			return 0;
			
		case 4:	// Flush ITLB Entry
		case 5: // Flush DTLB Entry
#ifdef DEBUG_SYSCALLS
			printf("syscall: flush i/d entry %x\n", (uint32_t)arg);
#endif
			captive::arch::CPU::get_active_cpu()->mmu().invalidate_virtual_mapping((gva_t)arg);
			return 0;
			
		case 6: // Invalidate I$
#ifdef DEBUG_SYSCALLS
			printf("syscall: invalidate I$\n");
#endif
			captive::arch::CPU::get_active_cpu()->invalidate_virtual_mappings();
			return 0;

		case 7: // Invalidate I$ Entry
#ifdef DEBUG_SYSCALLS
			printf("syscall: invalidate I$ %p\n", (uint32_t)arg);
#endif
			captive::arch::CPU::get_active_cpu()->invalidate_virtual_mapping((gva_t)arg);
			return 0;
			
		case 8: // Flush by Context ID
#ifdef DEBUG_SYSCALLS
			printf("syscall: flush ctx %x\n", (uint32_t)arg);
#endif
			captive::arch::CPU::get_active_cpu()->mmu().invalidate_virtual_mapping_by_context_id((uint32_t)arg);
			return 0;

		case 9: // Set Context ID
#ifdef DEBUG_SYSCALLS
			printf("syscall: set ctx %x\n", (uint32_t)arg);
#endif
			captive::arch::CPU::get_active_cpu()->mmu().context_id((uint32_t)arg);
			return 0;

		case 10: // Page Table Change
#ifdef DEBUG_SYSCALLS
			printf("syscall: pgt change\n",);
#endif
			captive::arch::CPU::get_active_cpu()->mmu().page_table_change();
			return 0;
		}
		
		return -1;
	}
}
