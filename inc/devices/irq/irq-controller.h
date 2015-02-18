/*
 * File:   irq-controller.h
 * Author: spink
 *
 * Created on 18 February 2015, 17:51
 */

#ifndef IRQ_CONTROLLER_H
#define	IRQ_CONTROLLER_H

#include <define.h>
#include <devices/irq/irq-line.h>

namespace captive {
	namespace devices {
		namespace irq {
			class IRQControllerBase
			{
				friend class IRQLine;

			public:
				IRQControllerBase();
				virtual ~IRQControllerBase();

			protected:
				virtual void irq_raised(IRQLine& line);
				virtual void irq_rescinded(IRQLine& line);
			};

			template<uint32_t nr_lines>
			class IRQController : public IRQControllerBase {
			public:
				IRQController();
				virtual ~IRQController();

				inline IRQLine *get_irq_line(uint32_t idx) {
					if (idx < nr_lines) {
						return &lines[idx];
					} else {
						return NULL;
					}
				}

			private:
				IRQLine lines[nr_lines];
			};
		}
	}
}

#endif	/* IRQ_CONTROLLER_H */

