/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   pit.h
 * Author: s0457958
 *
 * Created on 27 June 2016, 10:26
 */

#ifndef PIT_H
#define PIT_H

#include <define.h>

#define PIT_PORT	0x61

namespace captive
{
	namespace arch
	{
		namespace x86
		{
			class PIT
			{
			public:
				void initialise_oneshot(uint16_t ticks);
				
				void start();
				void stop();
				unsigned int expired();
			};
			
			extern PIT pit;
		}
	}
}

#endif /* PIT_H */

