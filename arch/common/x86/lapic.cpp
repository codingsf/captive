#include <x86/lapic.h>
#include <x86/pit.h>
#include <printf.h>
#include <map>

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
	lapic_write(TIMER, 0x20 | MASKED);
	
	timer_reset();
}

void LAPIC::calibrate(PIT& pit)
{
	#define PIT_FREQUENCY 		1193180
	#define CALIBRATION_FREQUENCY	100

	uint32_t period = (PIT_FREQUENCY / CALIBRATION_FREQUENCY);
	timer_reset();
	timer_set_periodic(false);

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
	lapic_clear(TIMER, MASKED);
}

void LAPIC::timer_stop()
{
	lapic_set(TIMER, MASKED);
}

void LAPIC::timer_reset(uint32_t init_val)
{
	lapic_set(TIMER, MASKED);
	lapic_write(TDCR, 3);
	lapic_write(TICR, init_val);
}

void LAPIC::timer_set_periodic(bool periodic)
{
	if (periodic) {
		lapic_set(TIMER, PERIODIC);
	} else {
		lapic_clear(TIMER, PERIODIC);
	}
}

#include <device.h>

namespace captive
{
	namespace arch
	{
		uint64_t __page_fault;
	}
}

extern "C"
{
	void handle_trap_timer(struct mcontext *mc)
	{
		/*if (mc->rip > 0xffffffff00000000) {
			printf("Q: %016lx\n", mc->rip);
			
			/*if (mc->rip >= (uintptr_t)&captive::arch::mmio_device_read && mc->rip < (uintptr_t)&captive::arch::mmio_device_dummy) {
				//printf("SAMPLE: BARRIER\n");
			} else {
				printf("SAMPLE: INFRASTRUCTURE\n");
			}* /
			
		} else {
			//printf("SAMPLE: NATIVE\n");
		}*/
				
		printf("PAGE FAULT: %lu\n", __page_fault);
		__page_fault = 0;
		
		lapic.acknowledge_irq();
		
		lapic.timer_reset(lapic.frequency()*100);
		lapic.timer_start();
	}
}

LAPIC captive::arch::x86::lapic;
