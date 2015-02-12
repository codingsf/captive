/*
 * File:   assert.h
 * Author: spink
 *
 * Created on 12 February 2015, 12:01
 */

#ifndef ASSERT_H
#define	ASSERT_H

#define assert(_expr) if (!_expr) asm volatile("out %0, $0xff\n" :: "a"(3));

#endif	/* ASSERT_H */

