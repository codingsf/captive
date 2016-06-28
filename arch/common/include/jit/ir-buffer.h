/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   ir-buffer.h
 * Author: s0457958
 *
 * Created on 28 June 2016, 09:48
 */

#ifndef IR_BUFFER_H
#define IR_BUFFER_H

#include <shared-jit.h>

namespace captive
{
	namespace arch
	{
		namespace jit
		{
			class IRBuffer
			{
			public:
				IRBuffer();
				
				shared::IRInstruction *at(int index) const { return &_current[index]; }
				
				void swap(int a, int b) {
					shared::IRInstruction temp = _current[a];
					_current[a] = _current[b];
					_current[b] = temp;
				}
				
				shared::IRInstruction *current_buffer() const { return _current; }
				shared::IRInstruction *other_buffer() const { return ((uintptr_t)_current == _buffer_a) ? _buffer_b : _buffer_a; }

				void reset_buffer() { _current = _buffer_a; }
				void swap_buffer() { if ((uintptr_t)_current == (uintptr_t)_buffer_a) _current = _buffer_b; else _current = _buffer_a; }
				
			private:
				shared::IRInstruction *_buffer_a, *_buffer_b, *_current;
			};
		}
	}
}

#endif /* IR_BUFFER_H */

