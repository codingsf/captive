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

#define LAPIC ((volatile uint32_t *)0x67fffee00000ULL)

// Local APIC registers, divided by 4 for use as uint[] indices.
#define ID      (0x0020)   // ID
#define VER     (0x0030)   // Version
#define TPR     (0x0080)   // Task Priority
#define EOI     (0x00B0)   // EOI
#define SVR     (0x00F0)   // Spurious Interrupt Vector
  #define ENABLE     0x00000100   // Unit Enable
#define ESR     (0x0280)   // Error Status
#define ICRLO   (0x0300)   // Interrupt Command
  #define INIT       0x00000500   // INIT/RESET
  #define STARTUP    0x00000600   // Startup IPI
  #define DELIVS     0x00001000   // Delivery status
  #define ASSERT     0x00004000   // Assert interrupt (vs deassert)
  #define DEASSERT   0x00000000
  #define LEVEL      0x00008000   // Level triggered
  #define BCAST      0x00080000   // Send to all APICs, including self.
  #define BUSY       0x00001000
  #define FIXED      0x00000000
#define ICRHI   (0x0310)   // Interrupt Command [63:32]
#define TIMER   (0x0320)   // Local Vector Table 0 (TIMER)
  #define X1         0x0000000B   // divide counts by 1
  #define PERIODIC   0x00020000   // Periodic
#define PCINT   (0x0340)   // Performance Counter LVT
#define LINT0   (0x0350)   // Local Vector Table 1 (LINT0)
#define LINT1   (0x0360)   // Local Vector Table 2 (LINT1)
#define ERROR   (0x0370)   // Local Vector Table 3 (ERROR)
  #define MASKED     0x00010000   // Interrupt masked
#define TICR    (0x0380)   // Timer Initial Count
#define TCCR    (0x0390)   // Timer Current Count
#define TDCR    (0x03E0)   // Timer Divide Configuration

namespace captive
{
	namespace arch
	{
		extern void lapic_init_irqs();
		extern void lapic_acknowledge_irq();
		extern void smp_init_cpu(int cpu);
		extern void smp_cpu_start(int cpu);
	}
}

#endif /* SMP_H */
