/*
 * File:   irq-line.h
 * Author: spink
 *
 * Created on 18 February 2015, 17:40
 */

#ifndef IRQ_LINE_H
#define	IRQ_LINE_H

#include <atomic>

namespace captive {
	namespace devices {
		namespace irq {
			class IRQControllerBase;

			class IRQLine
			{
			public:
				IRQLine();

				void raise();
				void rescind();

				inline bool raised() const { return _raised; }
				inline uint32_t index() const { return _index; }

				inline void attach(IRQControllerBase& controller, uint32_t index) {
					_controller = &controller;
					_index = index;
				}

			private:
				std::atomic<bool> _raised;
				uint32_t _index;
				IRQControllerBase *_controller;
			};
		}
	}
}

#endif	/* IRQ_LINE_H */

