#include <smp.h>
#include <printf.h>

namespace captive
{
	namespace arch
	{
		static void apic_write(uint32_t reg, uint32_t value)
		{
			LAPIC[reg >> 2] = value;
			LAPIC[1];
		}

		static uint32_t apic_read(uint32_t reg)
		{
			return LAPIC[reg >> 2];
		}

		void lapic_init_irqs()
		{
			uint64_t lapic_msr_val = __rdmsr(0x1b);
			printf("lapic msr=%lx, base=%p\n", lapic_msr_val, LAPIC);
			
			apic_write(SVR, 0x1ff);

			apic_write(TIMER, MASKED);
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
			apic_write(TICR, 1);
		}
		
		void lapic_acknowledge_irq()
		{
			apic_write(EOI, 0);
		}
		
		void smp_init_cpu(int tgt)
		{
			apic_write(ICRHI, (tgt & 0xf) << 24);
			apic_write(ICRLO, 0x4500);
		}
		
		void smp_cpu_start(int tgt)
		{
			apic_write(ICRHI, (tgt & 0xf) << 24);
			apic_write(ICRLO, 0x4600 | 0);
		}
	}
}
