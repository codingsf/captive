/*
 * File:   printf.h
 * Author: spink
 *
 * Created on 10 February 2015, 11:49
 */

#ifndef PRINTF_H
#define	PRINTF_H

#include <define.h>
#include <stdarg.h>

extern void printf_init(uintptr_t base_address, size_t size);

extern int printf(const char *fmt, ...);
extern int fatal(const char *fmt, ...) __attribute__((noreturn));
extern int sprintf(char *buffer, const char *fmt, ...);
extern int scatf(char *buffer, const char *fmt, ...);
extern int snprintf(char *buffer, int size, const char *fmt, ...);
extern int vsnprintf(char *buffer, int size, const char *fmt, va_list args);

#endif	/* PRINTF_H */

