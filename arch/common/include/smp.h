/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   smp.h
 * Author: s0457958
 *
 * Created on 11 January 2016, 11:55
 */

#ifndef SMP_H
#define SMP_H

#include <define.h>

namespace captive
{
	namespace arch
	{
		extern void smp_init_cpu(int cpu);
		extern void smp_cpu_start(int cpu);
	}
}

#endif /* SMP_H */

