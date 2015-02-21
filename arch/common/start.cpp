/*
 * start.c
 */
#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>
#include <shmem.h>

volatile uint32_t page_fault_code;
volatile captive::shmem_data *shmem;

extern captive::arch::Environment *create_environment();

#define APIC_BASE (volatile uint32_t *)0x200001000

static inline void write_ioapic_register(volatile uint32_t *apic_base, const uint8_t offset, const uint32_t val)
{
	apic_base[0] = offset;
	apic_base[4] = val;
}

static inline uint32_t read_ioapic_register(volatile uint32_t *apic_base, const uint8_t offset)
{
	apic_base[0] = offset;
	return apic_base[4];
}

static void init_ioapic()
{
	printf("ioapic: %d\n", read_ioapic_register(APIC_BASE, 0));
}

extern "C" {

	void __attribute__((noreturn)) start(uint64_t first_phys_page, unsigned int ep)
	{
		init_ioapic();

		shmem = (captive::shmem_data *)0x210000000;

		captive::arch::Memory mm(first_phys_page);
		captive::arch::Environment *env = create_environment();

		page_fault_code = 0;

		if (!env) {
			printf("error: unable to create environment\n");
		} else {
			if (!env->init()) {
				printf("error: unable to initialise environment\n");
			} else if (!env->run(ep)) {
				printf("error: unable to launch environment\n");
			}

			delete env;
		}

		abort();
	}

	void handle_trap_unk()
	{
		printf("IT'S A TRAP\n");
		abort();
	}

	void handle_trap_dbz()
	{
		printf("trap: divide-by-zero\n");
		abort();
	}

	void handle_trap_dbg()
	{
		printf("trap: debug\n");
		abort();
	}

	void handle_trap_nmi()
	{
		printf("trap: nmi\n");
		abort();
	}

	void handle_trap_gpf(uint64_t rip, uint64_t code)
	{
		printf("general protection fault: rip=%x\n", rip);
		abort();
	}

	void handle_trap_pf(uint64_t rip, uint64_t code)
	{
		uint64_t va;
		asm ("mov %%cr2, %0\n" : "=r"(va));

		if (va < 0x100000000) {
			captive::arch::CPU *core = captive::arch::active_cpu;
			if (core) {
				if (core->mmu().handle_fault((captive::arch::va_t)va)) {
					//printf("trap: handled page-fault: rip=%x va=%x, code=%x, pc=%x\n", rip, va, code, core->read_pc());
					return;
				} else {
					printf("trap: unhandled page-fault: rip=%x va=%x, code=%x, pc=%x\n", rip, va, code, core->read_pc());
					abort();
				}
			} else {
				printf("trap: unhandled page-fault: rip=%x va=%x, code=%x, (no cpu)\n", rip, va, code);
				abort();
			}
		} else {
			printf("trap: internal page-fault: rip=%x va=%x, code=%x\n", rip, va, code);
			abort();
		}
	}

	void handle_trap_irq()
	{
		printf("trap: irq\n");
		abort();
	}
}
