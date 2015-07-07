/*
 * File:   priv.h
 * Author: spink
 *
 * Created on 13 March 2015, 10:35
 */

#ifndef PRIV_H
#define	PRIV_H

static inline bool in_kernel_mode()
{
	unsigned short cs;
	asm volatile("mov %%cs, %0" : "=r"(cs));
	return (cs & 3) == 0;
}

static inline bool in_user_mode()
{
	unsigned short cs;
	asm volatile("mov %%cs, %0" : "=r"(cs));
	return (cs & 3) == 3;
}

static inline void switch_to_kernel_mode(void)
{
	//if (!in_kernel_mode())
		asm("int $0x80\n");
}

static inline void switch_to_user_mode(void)
{
	//if (!in_user_mode())
		asm("int $0x81\n");
}

#endif	/* PRIV_H */
