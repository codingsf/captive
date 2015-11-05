/*
 * File:   assert.h
 * Author: spink
 *
 * Created on 12 February 2015, 12:01
 */

#ifndef ASSERT_H
#define	ASSERT_H

struct mctx;

extern void __assertion_failure(const char *filename, int lineno, const char *expression) __attribute__((noreturn));

extern void dump_code(unsigned long int rip);
extern void dump_stack(void);
extern void dump_mcontext(const struct mcontext *mctx);

//#define assert(_expr) do { if (!(_expr)) { __assertion_failure(__FILE__, __LINE__, #_expr); } } while(0)
#define assert(_expr) 0

#endif	/* ASSERT_H */

