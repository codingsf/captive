/*
 * start.c
 */
#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>
#include <shmem.h>

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

typedef void (*func_ptr)(void);

//extern func_ptr _init_array_start[0], _init_array_end[0];
//extern func_ptr _fini_array_start[0], _fini_array_end[0];

static void call_static_constructors()
{
	//
}

extern "C" {
	void __attribute__((noreturn)) start(uint64_t first_phys_page, unsigned int ep)
	{
		call_static_constructors();
		init_ioapic();

		shmem = (captive::shmem_data *)0x210000000;

		captive::arch::Memory mm(first_phys_page);
		captive::arch::Environment *env = create_environment();

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

		printf("done\n");
		abort();
	}

	void handle_trap_unk(uint64_t rip)
	{
		printf("IT'S A TRAP @ %x\n", rip);
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

	void handle_trap_irq()
	{
		printf("trap: irq\n");
		abort();
	}
}
