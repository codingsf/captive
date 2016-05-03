/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   ring.h
 * Author: s0457958
 *
 * Created on 28 April 2016, 15:03
 */

#ifndef RING_H
#define RING_H

namespace captive
{
	namespace ring
	{
		template<typename elem_t, uint32_t count>
		struct RingBuffer
		{
			uint64_t head, tail;
			elem_t elements[count];
		};
	}
}

#endif /* RING_H */

