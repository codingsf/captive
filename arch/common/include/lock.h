/*
 * File:   lock.h
 * Author: spink
 *
 * Created on 26 March 2015, 18:51
 */

#ifndef LOCK_H
#define	LOCK_H

#include <define.h>

typedef volatile uint64_t spinlock_t;

extern "C" void spin_lock(spinlock_t *lock);
extern "C" void spin_unlock(spinlock_t *lock);

#endif	/* LOCK_H */

