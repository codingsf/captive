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

static void init_irqs()
{
	/*volatile uint8_t *lapic = (volatile uint8_t *)0x280001000;

	lapic[0xf0] = 0xff;*/
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

		// Initialise IRQs.
		init_irqs();

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

	void handle_trap_unk(struct mcontext *mctx)
	{
		printf("unhandled exception: rip=0x%lx\n", mctx->rip);
		abort();
	}

	void handle_trap_unk_arg(struct mcontext *mctx)
	{
		printf("unhandled exception: rip=0x%lx, code=0x%lx\n", mctx->rip, mctx->extra);
		abort();
	}

	void handle_trap_gpf(struct mcontext *mctx)
	{
		printf("general protection fault: rip=0x%lx, code=0x%x\n", mctx->rip, mctx->extra);
		abort();
	}

	void handle_trap_irq(struct mcontext *mctx)
	{
		printf("irq!\n");
		abort();
	}

	void handle_signal(struct mcontext *mctx)
	{
		printf("signal %d\n", captive::arch::CPU::get_active_cpu()->cpu_data().signal_code);
	}
}
