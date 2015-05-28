#include <syscall.h>
#include <printf.h>
#include <cpu.h>
#include <priv.h>

extern "C" {
	uint32_t do_syscall(struct mcontext *mctx)
	{
		switch (mctx->rdi) {
		case 0:
			captive::arch::CPU::get_active_cpu()->mmu().invalidate((gva_t)mctx->rsi);
			return 0;
		case 1:
			captive::arch::CPU::get_active_cpu()->mmu().invalidate((gva_t)mctx->rsi);
			return 0;
		}
		return -1;
	}
}
