#include <x86/lapic.h>
#include <x86/pit.h>
#include <mm.h>
#include <printf.h>
#include <map>

using namespace captive::arch;
using namespace captive::arch::x86;

void LAPIC::initialise()
{
	Memory::disable_caching((hva_t)LAPIC_MEM_BASE);
	
	uint64_t lapic_msr_val = __rdmsr(0x1b);
	printf("lapic: msr=%lx, base=%p\n", lapic_msr_val, LAPIC_MEM_BASE);

	lapic_write(SVR, 0x1ff);

	lapic_write(TIMER, MASKED);
	lapic_write(LINT0, MASKED);
	lapic_write(LINT1, MASKED);
	lapic_write(ERROR, MASKED);
	
	if (((lapic_read(VER) >> 16) & 0xFF) >= 4)
		lapic_write(PCINT, MASKED);

	lapic_write(ESR, 0);
	lapic_write(ESR, 0);

	lapic_write(EOI, 0);

	lapic_write(ICRHI, 0);
	lapic_write(ICRLO, BCAST | INIT | LEVEL);

	while (lapic_read(ICRLO) & DELIVS);

	lapic_write(TDCR, 3);
	lapic_write(TIMER, 0x20 | MASKED | PERIODIC);
	lapic_write(TICR, 1);

	lapic_write(TPR, 0);
	
	printf("lapic: id=%d, timer=%08x\n", lapic_read(ID), lapic_read(TIMER));
	
	timer_reset();
}

void LAPIC::calibrate(PIT& pit)
{
	#define FACTOR					1000
	#define PIT_FREQUENCY			(1193180)
	#define CALIBRATION_PERIOD		(10)
	#define CALIBRATION_TICKS		(uint16_t)((PIT_FREQUENCY * CALIBRATION_PERIOD) / FACTOR)

	timer_reset();
	timer_set_periodic(false);

	printf("pit: ticks=%d\n", (uint32_t)CALIBRATION_TICKS);
	pit.initialise_oneshot(CALIBRATION_TICKS);		// 10ms
	pit.start();

	timer_start();
	while (!pit.expired());
	timer_stop();

	pit.stop();

	uint32_t ticks_per_period = (0xffffffff - timer_read());
	ticks_per_period <<= 4;
	
	printf("lapic: ticks-per-period=%d\n", ticks_per_period);

	_frequency = (ticks_per_period * (FACTOR/CALIBRATION_PERIOD));

	printf("lapic: frequency=%lu\n", _frequency);
}

void LAPIC::timer_start()
{
	printf("before:%08x\n", lapic_read(TIMER));
	lapic_clear(TIMER, MASKED);
	printf("after:%08x\n", lapic_read(TIMER));
}

void LAPIC::timer_stop()
{
	lapic_set(TIMER, MASKED);
}

void LAPIC::timer_reset(uint32_t init_val)
{
	lapic_set(TIMER, MASKED);
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

void LAPIC::timer_oneshot(uint32_t milliseconds)
{
	lapic_set(TIMER, MASKED);
	lapic_clear(TIMER, PERIODIC);
	lapic_write(TICR, ((_frequency >> 4) * milliseconds) / 1000);
	lapic_clear(TIMER, MASKED);
}

#include <device.h>

namespace captive
{
	namespace arch
	{
		uint64_t __page_faults;
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
				
		/*printf("PAGE FAULTS: %lu\n", __page_faults);
		__page_faults = 0;*/
		
		//printf("XXXXX\n");
		lapic.acknowledge_irq();
		//lapic.timer_oneshot(1000);
	}
}

LAPIC captive::arch::x86::lapic;
