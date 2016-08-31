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
	namespace hypervisor {
		class CPU;
	}

	namespace devices {
		namespace irq {
			class IRQControllerBase
			{
				friend class IRQLineBase;

			public:
				IRQControllerBase();
				virtual ~IRQControllerBase();

				virtual void dump() const = 0;

			protected:
				virtual void irq_raised(IRQLineBase& line) = 0;
				virtual void irq_rescinded(IRQLineBase& line) = 0;
				virtual void irq_acknowledged(IRQLineBase& line);
			};

			class CPUIRQController {
			public:
				inline void attach(hypervisor::CPU& cpu) {
					_cpu = &cpu;
				}

			protected:
				inline hypervisor::CPU& cpu() const { return *_cpu; }
				
			private:
				hypervisor::CPU *_cpu;
			};

			template<typename irq_line_t, uint32_t nr_lines>
			class IRQController : public IRQControllerBase {
			public:
				IRQController();
				virtual ~IRQController();

				inline irq_line_t *get_irq_line(uint32_t idx) {
					if (idx < nr_lines) {
						return &lines[idx];
					} else {
						return NULL;
					}
				}
				
				inline const irq_line_t *get_irq_line(uint32_t idx) const {
					if (idx < nr_lines) {
						return &lines[idx];
					} else {
						return NULL;
					}
				}

				virtual void dump() const override;
				
				inline constexpr uint32_t count() const { return nr_lines; }

			private:
				irq_line_t lines[nr_lines];
			};
		}
	}
}

#endif	/* IRQ_CONTROLLER_H */

