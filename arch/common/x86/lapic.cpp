#include <x86/lapic.h>
#include <x86/pit.h>
#include <printf.h>

using namespace captive::arch;
using namespace captive::arch::x86;

void LAPIC::initialise()
{
	uint64_t lapic_msr_val = __rdmsr(0x1b);
	printf("lapic msr=%lx, base=%p\n", lapic_msr_val, LAPIC_MEM_BASE);

	lapic_write(SVR, 0x1ff);

	lapic_write(TIMER, MASKED);
	lapic_write(LINT0, MASKED);
	lapic_write(LINT1, MASKED);
	lapic_write(ERROR, MASKED);

	lapic_write(ESR, 0);
	lapic_write(ESR, 0);

	lapic_write(EOI, 0);

	lapic_write(ICRHI, 0);
	lapic_write(ICRLO, BCAST | INIT | LEVEL);

	while (lapic_read(ICRLO) & DELIVS);

	lapic_write(TPR, 0);
	
	timer_reset();
}

void LAPIC::calibrate(PIT& pit)
{
	#define PIT_FREQUENCY 		1193180
	#define CALIBRATION_FREQUENCY	100

	uint32_t period = (PIT_FREQUENCY / CALIBRATION_FREQUENCY);
	timer_reset();

	pit.initialise_oneshot(period);
	pit.start();

	timer_start();
	while (!pit.expired());
	timer_stop();

	pit.stop();

	uint32_t ticks_per_period = (0xffffffff - timer_read());
	ticks_per_period <<= 4;

	_frequency = ticks_per_period / 100;
}

void LAPIC::timer_start()
{
	lapic_write(TIMER, 32);
}

void LAPIC::timer_stop()
{
	lapic_write(TIMER, MASKED);
}

void LAPIC::timer_reset()
{
	lapic_write(TIMER, MASKED);
	lapic_write(TDCR, 3);
	lapic_write(TICR, 0xffffffff);
}

extern "C"
{
	void handle_trap_timer(struct mcontext *mc)
	{
		captive::arch::x86::lapic.acknowledge_irq();
	}
}

LAPIC captive::arch::x86::lapic;
