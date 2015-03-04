/*
 * File:   assert.h
 * Author: spink
 *
 * Created on 12 February 2015, 12:01
 */

#ifndef ASSERT_H
#define	ASSERT_H

#include <printf.h>

#define assert(_expr) do { if (!_expr) { printf("ABORT: " __FILE__ ": %d: " #_expr "\n", __LINE__); asm volatile("out %0, $0xff\n" :: "a"(3)); } } while(0)

#endif	/* ASSERT_H */

