/*
 * File:   ps2.h
 * Author: spink
 *
 * Created on 23 February 2015, 12:26
 */

#ifndef PS2_H
#define	PS2_H

#include <devices/io/keyboard.h>
#include <devices/io/mouse.h>
#include <devices/irq/irq-line.h>

#include <queue>

namespace captive {
	namespace devices {
		namespace io {
			class PS2Device
			{
			public:
				PS2Device(irq::IRQLine& irq);

				inline uint32_t read() const {
					uint32_t v = data_queue.front();
					data_queue.pop();
					if (data_queue.empty()) {
						_irq.rescind();
					}

					return v;
				}

				inline bool data_pending() const { return data_queue.size() > 0; }

				virtual void send_command(uint32_t command) = 0;

				inline void enable_irq() { irq_enabled = true; }
				inline void disable_irq() { irq_enabled = false; }

			protected:
				inline void queue_data(uint32_t data) {
					data_queue.push(data);
					if (irq_enabled) {
						_irq.raise();
					}
				}

			private:
				irq::IRQLine& _irq;
				bool irq_enabled;
				mutable std::queue<uint32_t> data_queue;
			};

			class PS2KeyboardDevice : public PS2Device, public Keyboard
			{
			public:
				PS2KeyboardDevice(irq::IRQLine& irq);

				void send_command(uint32_t command) override;

				void key_down(uint32_t keycode) override;
				void key_up(uint32_t keycode) override;

			private:
				uint32_t last_command;
			};

			class PS2MouseDevice : public PS2Device, public Mouse
			{
			public:
				PS2MouseDevice(irq::IRQLine& irq);
				void send_command(uint32_t command) override;

				void button_down(uint32_t button_index) override;
				void button_up(uint32_t button_index) override;
				void mouse_move(uint32_t x, uint32_t y) override;

			private:
				uint32_t last_command;

				uint32_t status, resolution, sample_rate;

				uint32_t last_x, last_y;
				uint32_t button_state;
			};
		}
	}
}

#endif	/* PS2_H */

