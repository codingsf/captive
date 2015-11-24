/*
 * File:   cpu-irq.h
 * Author: spink
 *
 * Created on 18 February 2015, 18:13
 */

#ifndef CPU_IRQ_H
#define	CPU_IRQ_H

#include <devices/irq/irq-controller.h>

namespace captive {
	namespace devices {
		namespace arm {
			class ArmCpuIRQController : public irq::IRQController<2u>, public irq::CPUIRQController
			{
			public:
				void irq_raised(irq::IRQLine& line) override;
				void irq_rescinded(irq::IRQLine& line) override;
				void irq_acknowledged(irq::IRQLine& line) override;
			};
		}
	}
}

#endif	/* CPU_IRQ_H */

