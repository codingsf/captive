/*
 * File:   mouse.h
 * Author: spink
 *
 * Created on 23 February 2015, 12:26
 */

#ifndef MOUSE_H
#define	MOUSE_H

#include <define.h>

namespace captive {
	namespace devices {
		namespace io {
			class Mouse
			{
			public:
				virtual void button_down(uint32_t btncode) = 0;
				virtual void button_up(uint32_t btncode) = 0;
				virtual void mouse_move(uint32_t x, uint32_t y) = 0;
			};
		}
	}
}

#endif	/* MOUSE_H */

