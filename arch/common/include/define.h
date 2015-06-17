/*
 * File:   define.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:46
 */

#ifndef DEFINE_H
#define	DEFINE_H

#include <assert.h>

#define packed __attribute__((packed))

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))

typedef unsigned long size_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int int64_t;

typedef unsigned long ptrdiff_t;

typedef void *pa_t;
typedef void *va_t;

typedef uint32_t gpa_t;
typedef uint32_t gva_t;

#define NULL 0

struct mcontext
{
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp;
	uint64_t rsi, rdi, rdx, rcx, rbx, rax;
	uint64_t flags, extra, rip;
} packed;

static inline __attribute__((noreturn)) void abort()
{
	asm volatile("out %0, $0xff\n" : : "a"(0x02));

	for(;;) {
		asm volatile("hlt\n");
	}
}

static inline void __local_irq_enable()
{
	asm volatile("sti\n");
}

static inline void __local_irq_disable()
{
	asm volatile("cli\n");
}

#endif	/* DEFINE_H */

