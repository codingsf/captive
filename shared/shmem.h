/*
 * File:   shmem.h
 * Author: spink
 *
 * Created on 20 February 2015, 19:11
 */

#ifndef SHMEM_H
#define	SHMEM_H

namespace captive {
	namespace lock {
		typedef volatile uint64_t SpinLock;

		inline void spinlock_init(SpinLock *lock)
		{
			*lock = 0;
		}

		inline void spinlock_acquire(SpinLock *lock)
		{
			while(__sync_lock_test_and_set(lock, 1)) asm volatile("pause" ::: "memory");
		}

		inline void spinlock_release(SpinLock *lock)
		{
			__sync_synchronize();
			*lock = 0;
		}
	}

	struct PerGuestData {
		uint32_t entrypoint;
		
		uintptr_t printf_buffer;
		
		uintptr_t heap_virt_base;
		uintptr_t heap_phys_base;
		size_t heap_size;
	};

	struct PerCPUData {
		uint64_t id;
		
		PerGuestData *guest_data;
				
		bool halt;

		uint32_t isr;				// Interrupt Status Register
		uint32_t async_action;		// Pending actions
		uint32_t signal_code;		// Incoming signal code
		uint64_t insns_executed;	// Number of instructions executed
		uint64_t interrupts_taken;
		
		uint32_t execution_mode;	// Mode of execution

		bool verbose_enabled;
	};
}

#endif	/* SHMEM_H */
