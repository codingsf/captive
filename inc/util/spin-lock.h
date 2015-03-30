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

namespace captive {
	namespace util {
		class SpinLockWrapper
		{
		public:
			SpinLockWrapper(spinlock_t *lock) : _lock(lock) { spin_lock(_lock); }
			~SpinLockWrapper() { spin_unlock(_lock); }

		private:
			spinlock_t *_lock;
		};
	}
}

#endif	/* SPIN_LOCK_H */

