/*
 * File:   safepoint.h
 * Author: spink
 *
 * Created on 24 February 2015, 12:04
 */

#ifndef SAFEPOINT_H
#define	SAFEPOINT_H

#include <define.h>

typedef struct safepoint {
	uint64_t rbx, rsp, rbp, r12, r13, r14, r15, rip, rflags;
} safepoint_t;

extern "C" int record_safepoint(safepoint_t *sp);
extern "C" int interrupt_restore_safepoint(safepoint_t *sp, int v);

extern "C" void switch_to_ring3(void);
extern "C" void switch_to_ring0(void);

static inline int current_ring() {
	unsigned short cs;
	asm volatile("mov %%cs, %0" : "=r"(cs));
	return cs & 3;
}

#endif	/* SAFEPOINT_H */

