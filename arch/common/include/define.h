/*
 * File:   define.h
 * Author: spink
 *
 * Created on 10 February 2015, 12:46
 */

#ifndef DEFINE_H
#define	DEFINE_H

#include <assert.h>

typedef unsigned long size_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int int64_t;

#define NULL 0

#define abort() asm volatile("out %0, $0xff\n" : : "a"(0x02))

#endif	/* DEFINE_H */

