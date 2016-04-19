#include <syscall.h>
#include <printf.h>
#include <cpu.h>
#include <priv.h>

//#define DEBUG_SYSCALLS

extern "C" {
	void do_fast_syscall(struct mcontext *mctx, uint64_t arg, uint64_t __unused, uint64_t syscall_nr)
	{
		switch (syscall_nr) {
		case 1: // Flush
		case 2: // Flush ITLB
		case 3: // Flush DTLB
			captive::arch::CPU::get_active_cpu()->mmu().invalidate_virtual_mappings();
			break;

		case 4:	// Flush ITLB Entry
		case 5: // Flush DTLB Entry
			captive::arch::CPU::get_active_cpu()->mmu().invalidate_virtual_mapping((gva_t)arg);
			break;

		case 6: // Invalidate I$
			//printf("YYY\n");
			//captive::arch::CPU::get_active_cpu()->invalidate_translations();
			break;

		case 7: // Invalidate I$ Entry
			//printf("XXX %08x\n", arg);
			//captive::arch::CPU::get_active_cpu()->invalidate_translation_virt(arg);
			break;

		case 8: // Flush by Context ID
			captive::arch::CPU::get_active_cpu()->mmu().invalidate_virtual_mapping_by_context_id((uint32_t)arg);
			break;

		case 9: // Set Context ID
			captive::arch::CPU::get_active_cpu()->mmu().context_id((uint32_t)arg);
			break;

		case 10: // Page Table Change
			captive::arch::CPU::get_active_cpu()->mmu().page_table_change();
			break;
			
		case 11:
			printf("SDFSDFSDFSDFSFDFS\n");
			captive::arch::Memory::flush_tlb();
			break;
		}
	}
}
