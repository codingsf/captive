/*
 * File:   spin-lock.h
 * Author: spink
 *
 * Created on 27 March 2015, 08:38
 */

#ifndef SPIN_LOCK_H
#define	SPIN_LOCK_H

#include <shmem.h>

namespace captive {
	namespace util {
		class SpinLockWrapper
		{
		public:
			SpinLockWrapper(captive::lock::SpinLock *lock) : _lock(lock) { captive::lock::spinlock_acquire(_lock); }
			~SpinLockWrapper() { captive::lock::spinlock_release(_lock); }

		private:
			captive::lock::SpinLock *_lock;
		};
	}
}

#endif	/* SPIN_LOCK_H */

