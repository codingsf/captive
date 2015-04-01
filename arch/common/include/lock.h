/*
 * File:   lock.h
 * Author: spink
 *
 * Created on 26 March 2015, 18:51
 */

#ifndef LOCK_H
#define	LOCK_H

#include <define.h>
#include <shmem.h>

namespace captive
{
	namespace arch
	{
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

#endif	/* LOCK_H */

