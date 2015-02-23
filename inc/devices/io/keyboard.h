/*
 * File:   keyboard.h
 * Author: spink
 *
 * Created on 23 February 2015, 12:26
 */

#ifndef KEYBOARD_H
#define	KEYBOARD_H

#include <define.h>

namespace captive {
	namespace devices {
		namespace io {
			class Keyboard
			{
			public:
				virtual void key_down(uint32_t keycode) = 0;
				virtual void key_up(uint32_t keycode) = 0;
			};
		}
	}
}

#endif	/* KEYBOARD_H */

