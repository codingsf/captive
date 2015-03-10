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

volatile captive::shmem_data *shmem;

extern captive::arch::Environment *create_environment();

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

/*static inline rdpcpu(uint32_t offset)
{

	asm volatile("mov %fs:0, %0" : "=r"(val));
}*/

#define offsetof(st, m) __builtin_offsetof(st, m)
#define __GET_PER_CPU_DATA(_member, _offset) asm volatile("mov %%fs:%1, %0" : "=r"(val) : "i"(_offset))
#define GET_PER_CPU_DATA(_member) __GET_PER_CPU_DATA(_member, offsetof(struct captive::per_cpu_data, _member))

extern "C" {
	void __attribute__((noreturn)) start_environment(captive::per_cpu_data *i_cpu_data)
	{
		// Populate the FS register with the address of the Per-CPU data structure.
		wrmsr(0xc0000100, (uint64_t)i_cpu_data);

		volatile captive::per_cpu_data *cpu_data asm ("%gs:0") = i_cpu_data;

		// Run the static constructors.
		call_static_constructors();

		// Initialise the malloc() memory allocation system.
		captive::arch::malloc_init(cpu_data->heap, cpu_data->heap_size);

		// Store a pointer to the global shared memory area.
		shmem = cpu_data->shmem_area;

		// Initialise the memory manager, and create the environment.
		captive::arch::Memory mm(cpu_data->first_avail_phys_page);
		captive::arch::Environment *env = create_environment();

		if (!env) {
			printf("error: unable to create environment\n");
		} else {
			if (!env->init()) {
				printf("error: unable to initialise environment\n");
			} else if (!env->run(cpu_data->guest_entrypoint, shmem->cpu_options.mode)) {
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
