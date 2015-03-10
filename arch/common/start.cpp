/*
 * start.c
 */
#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>
#include <malloc.h>
#include <shmem.h>

extern captive::arch::Environment *create_environment(captive::PerCPUData *per_cpu_data);

static void call_static_constructors()
{
	// TODO
}

static inline void wrmsr(uint32_t msr_id, uint64_t msr_value)
{
	uint32_t low = msr_value & 0xffffffff;
	uint32_t high = (msr_value >> 32);

	asm volatile ( "rex.b wrmsr" : : "c" (msr_id), "a" (low), "d" (high) );
}

extern "C" {
	void __attribute__((noreturn)) start_environment(captive::PerCPUData *cpu_data)
	{
		// Populate the FS register with the address of the Per-CPU data structure.
		wrmsr(0xc0000100, (uint64_t)cpu_data);

		// Run the static constructors.
		call_static_constructors();

		// Initialise the printf() system.
		printf_init((char *)cpu_data->guest_data->printf_buffer, cpu_data->guest_data->printf_buffer_size);

		// Initialise the malloc() memory allocation system.
		captive::arch::malloc_init(cpu_data->guest_data->heap, cpu_data->guest_data->heap_size);

		// Initialise the memory manager, and create the environment.
		captive::arch::Memory mm(cpu_data->guest_data->next_phys_page);
		captive::arch::Environment *env = create_environment(cpu_data);

		if (!env) {
			printf("error: unable to create environment\n");
		} else {
			if (!env->init()) {
				printf("error: unable to initialise environment\n");
			} else if (!env->run()) {
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
