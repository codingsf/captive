/*
 * start.c
 */
#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>
#include <smp.h>
#include <malloc/malloc.h>
#include <malloc/page-allocator.h>

extern captive::arch::Environment *create_environment_arm(captive::PerCPUData *per_cpu_data);

extern void (*__init_array_start []) (void);
extern void (*__init_array_end []) (void);

static void call_static_constructors()
{
	size_t size = __init_array_end - __init_array_start;

	for (unsigned int i = 0; i < size; i++) {
		(*__init_array_start[i])();
	}
}

extern void foo(void);

namespace captive { namespace arch {
extern int do_device_read(struct mcontext *);
extern int do_device_write(struct mcontext *);
} }

extern "C" {
	void __attribute__((noreturn)) start_run_core(captive::PerCPUData *cpu_data)
	{
		printf("run core waiting...\n");
		for (;;);
	}

	void __attribute__((noreturn)) start_boot_core(captive::PerCPUData *cpu_data)
	{
		// Run the static constructors.
		printf("calling static constructors...\n");
		call_static_constructors();

		// Initialise IRQs.
		printf("initialising irqs...\n");
		captive::arch::lapic_init_irqs();

		// Initialise the printf() system.
		printf("initialising printf @ %p\n", cpu_data->guest_data->printf_buffer);
		printf_init(cpu_data->guest_data->printf_buffer, 0x1000);

		// Initialise the malloc() memory allocation system.
		printf("initialising malloc @ pa=%p, va=%p, size=%x\n", cpu_data->guest_data->heap_phys_base, cpu_data->guest_data->heap_virt_base, cpu_data->guest_data->heap_size);
		captive::arch::malloc::page_alloc.init(cpu_data->guest_data->heap_virt_base, cpu_data->guest_data->heap_phys_base, cpu_data->guest_data->heap_size);
		
		// Initialise the memory manager.
		printf("initialising mmu...\n");
		captive::arch::Memory::init();
		
		// Initialise other CPUs
		/*printf("initialising cpu 1...\n");
		captive::arch::smp_init_cpu(1);
		captive::arch::smp_cpu_start(1);*/

		printf("creating environment...\n");
		captive::arch::Environment *env = create_environment_arm(cpu_data);

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
	
	void __attribute__((noreturn)) start_environment(captive::PerCPUData *cpu_data, int id)
	{
		printf("--------------------------------------------------------------------\n");
		printf("*** VCPU %d starting... (per_cpu_data=%p)\n", id, cpu_data);
	
		if (id == 0) start_boot_core(cpu_data);
		else start_run_core(cpu_data);
	}
	
	void handle_trap_unk(struct mcontext *mctx)
	{
		printf("unhandled exception: rip=0x%lx\n", mctx->rip);
		dump_mcontext(mctx);
		abort();
	}

	void handle_trap_unk_arg(struct mcontext *mctx)
	{
		printf("unhandled exception: rip=0x%lx, code=0x%lx\n", mctx->rip, mctx->extra);
		abort();
	}

	void handle_trap_gpf(struct mcontext *mctx)
	{
		captive::arch::CPU *cpu = captive::arch::CPU::get_active_cpu();

		if (cpu) {
			if (cpu->trace().enabled()) {
				cpu->trace().end_record();
			}
		}

		printf("general protection fault: rip=0x%lx, code=0x%x, pc=0x%08x, ir=%08x\n", mctx->rip, mctx->extra, cpu ? cpu->read_pc() : 0, cpu ? *((uint32_t *)(uintptr_t)cpu->read_pc()) : 0);
		dump_mcontext(mctx);
		dump_stack();
		abort();
	}
	
	void handle_trap_irq0(struct mcontext *mctx)
	{
		captive::arch::lapic_acknowledge_irq();

		captive::arch::CPU *cpu = captive::arch::CPU::get_active_cpu();
		switch (cpu->cpu_data().signal_code) {
		case 2:
			cpu->dump_state();
			break;
		}
	}
	
	void handle_trap_irq1(struct mcontext *mctx)
	{
		captive::arch::lapic_acknowledge_irq();
		
		//printf("*** RAISED\n");
		captive::arch::CPU::get_active_cpu()->handle_irq_raised(1);
	}
	
	void handle_trap_irq2(struct mcontext *mctx)
	{
		captive::arch::lapic_acknowledge_irq();
		
		//printf("*** RESCINDED\n");
		captive::arch::CPU::get_active_cpu()->handle_irq_rescinded(1);
	}

	void handle_trap_irq3(struct mcontext *mctx)
	{
		captive::arch::lapic_acknowledge_irq();
		
		//printf("*** ACKED\n");
		captive::arch::CPU::get_active_cpu()->handle_irq_acknowledged(1);
	}
	
	int handle_trap_illegal(struct mcontext *mctx)
	{
		switch (*(uint8_t *)mctx->rip) {
		case 0xc4: return captive::arch::do_device_read(mctx);
		case 0xc5: return captive::arch::do_device_write(mctx);
		default: dump_code(mctx->rip-20); fatal("illegal instruction %02x\n", *(uint8_t *)mctx->rip);
		}
	}

	void handle_signal(struct mcontext *mctx)
	{
		printf("signal %d\n", captive::arch::CPU::get_active_cpu()->cpu_data().signal_code);
	}
}
