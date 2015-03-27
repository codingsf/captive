/*
 * File:   spin-lock.h
 * Author: spink
 *
 * Created on 27 March 2015, 08:38
 */

#ifndef SPIN_LOCK_H
#define	SPIN_LOCK_H

typedef volatile uint64_t spinlock_t;

extern "C" void spin_lock(spinlock_t *lock);
extern "C" void spin_unlock(spinlock_t *lock);

#endif	/* SPIN_LOCK_H */

