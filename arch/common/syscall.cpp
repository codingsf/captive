#include <syscall.h>
#include <printf.h>
#include <cpu.h>

extern "C" {
	uint32_t do_syscall(uint32_t idx, uint32_t v)
	{
		switch (idx) {
		case 0:
			captive::arch::CPU::get_active_cpu()->mmu().invalidate((gva_t)v);
			return 0;
		case 1:
			captive::arch::CPU::get_active_cpu()->mmu().invalidate((gva_t)v);
			return 0;
		}
		return -1;
	}
}
