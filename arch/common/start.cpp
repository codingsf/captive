/*
 * start.c
 */
#include <printf.h>
#include <mm.h>
#include <env.h>
#include <cpu.h>
#include <mmu.h>
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

//static uint32_t volatile * const lapic = (uint32_t volatile * const)0x280002000;
#define lapic ((volatile uint32_t *)0x67fffee00000ULL)

// Local APIC registers, divided by 4 for use as uint[] indices.
#define ID      (0x0020)   // ID
#define VER     (0x0030)   // Version
#define TPR     (0x0080)   // Task Priority
#define EOI     (0x00B0)   // EOI
#define SVR     (0x00F0)   // Spurious Interrupt Vector
  #define ENABLE     0x00000100   // Unit Enable
#define ESR     (0x0280)   // Error Status
#define ICRLO   (0x0300)   // Interrupt Command
  #define INIT       0x00000500   // INIT/RESET
  #define STARTUP    0x00000600   // Startup IPI
  #define DELIVS     0x00001000   // Delivery status
  #define ASSERT     0x00004000   // Assert interrupt (vs deassert)
  #define DEASSERT   0x00000000
  #define LEVEL      0x00008000   // Level triggered
  #define BCAST      0x00080000   // Send to all APICs, including self.
  #define BUSY       0x00001000
  #define FIXED      0x00000000
#define ICRHI   (0x0310)   // Interrupt Command [63:32]
#define TIMER   (0x0320)   // Local Vector Table 0 (TIMER)
  #define X1         0x0000000B   // divide counts by 1
  #define PERIODIC   0x00020000   // Periodic
#define PCINT   (0x0340)   // Performance Counter LVT
#define LINT0   (0x0350)   // Local Vector Table 1 (LINT0)
#define LINT1   (0x0360)   // Local Vector Table 2 (LINT1)
#define ERROR   (0x0370)   // Local Vector Table 3 (ERROR)
  #define MASKED     0x00010000   // Interrupt masked
#define TICR    (0x0380)   // Timer Initial Count
#define TCCR    (0x0390)   // Timer Current Count
#define TDCR    (0x03E0)   // Timer Divide Configuration

static void apic_write(uint32_t reg, uint32_t value)
{
	lapic[reg >> 2] = value;
	lapic[1];
}

static uint32_t apic_read(uint32_t reg)
{
	return lapic[reg >> 2];
}

static void init_irqs()
{
	apic_write(SVR, 0x1ff);

	apic_write(LINT0, MASKED);
	apic_write(LINT1, MASKED);
	apic_write(ERROR, MASKED);

	apic_write(ESR, 0);
	apic_write(ESR, 0);

	apic_write(EOI, 0);

	apic_write(ICRHI, 0);
	apic_write(ICRLO, BCAST | INIT | LEVEL);

	while (apic_read(ICRLO) & DELIVS);

	apic_write(TPR, 0);
}

extern void foo(void);

namespace captive { namespace arch {
extern int do_device_read(struct mcontext *);
extern int do_device_write(struct mcontext *);
} }

extern "C" {
	void __attribute__((noreturn)) start_environment(captive::PerCPUData *cpu_data)
	{
		printf("no time for that now... cpu_data=%p, entry_point=%x\n", cpu_data, cpu_data->entrypoint);

		// Run the static constructors.
		printf("calling static constructors...\n");
		call_static_constructors();

		// Initialise IRQs.
		printf("initialising irqs...\n");
		init_irqs();

		// Initialise the printf() system.
		printf("initialising printf @ %p\n", cpu_data->guest_data->printf_buffer);
		printf_init(cpu_data->guest_data->printf_buffer, 0x1000);

		// Initialise the malloc() memory allocation system.
		printf("initialising malloc @ pa=%p, va=%p, size=%x\n", cpu_data->guest_data->heap_phys_base, cpu_data->guest_data->heap_virt_base, cpu_data->guest_data->heap_size);
		captive::arch::malloc::page_alloc.init(cpu_data->guest_data->heap_virt_base, cpu_data->guest_data->heap_phys_base, cpu_data->guest_data->heap_size);
		
		// Initialise the memory manager.
		printf("initialising mmu...\n");
		captive::arch::Memory::init();

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

		printf("general protection fault: rip=0x%lx, code=0x%x, pc=0x%08x, ir=%08x\n", mctx->rip, mctx->extra, cpu ? cpu->read_pc() : 0, cpu ? *((uint32_t *)cpu->read_pc()) : 0);
		dump_mcontext(mctx);
		dump_stack();
		abort();
	}
	
	void handle_trap_irq0(struct mcontext *mctx)
	{
		apic_write(EOI, 0);

		captive::arch::CPU *cpu = captive::arch::CPU::get_active_cpu();
		switch (cpu->cpu_data().signal_code) {
		case 2:
			cpu->dump_state();
			break;
		}
	}
	
	void handle_trap_irq1(struct mcontext *mctx)
	{
		apic_write(EOI, 0);
		
		//printf("*** RAISED\n");
		captive::arch::CPU::get_active_cpu()->handle_irq_raised(1);
	}
	
	void handle_trap_irq2(struct mcontext *mctx)
	{
		apic_write(EOI, 0);
		
		//printf("*** RESCINDED\n");
		captive::arch::CPU::get_active_cpu()->handle_irq_rescinded(1);
	}

	void handle_trap_irq3(struct mcontext *mctx)
	{
		apic_write(EOI, 0);
		
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
