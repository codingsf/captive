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
		typedef volatile uint64_t spinlock;

		inline void spinlock_init(spinlock *lock)
		{
			*lock = 0;
		}

		inline void spinlock_acquire(spinlock *lock)
		{
			while(__sync_lock_test_and_set(lock, 1)) asm volatile("pause" ::: "memory");
		}

		inline void spinlock_release(spinlock *lock)
		{
			__sync_synchronize();
			*lock = 0;
		}
		
		typedef struct { uint32_t ctr; volatile uint32_t b[2]; } barrier;
		
		inline void barrier_init(barrier *b)
		{
			b->ctr = 0;
			b->b[0] = 0;
			b->b[1] = 0;
		}
		
		inline void barrier_wait(barrier *br, uint32_t tid)
		{
			assert(tid == 0 || tid == 1);
			
			uint32_t ctr = br->ctr;
			uint32_t val = !ctr;
			
			br->b[tid] = val;
			while (br->b[!tid] != val) asm ("pause");
			
			br->ctr = !ctr;
		}
		
		inline void barrier_wait_nopause(barrier *br, uint32_t tid)
		{
			assert(tid == 0 || tid == 1);
			
			uint32_t ctr = br->ctr;
			uint32_t val = !ctr;
			
			br->b[tid] = val;
			while (br->b[!tid] != val);
			
			br->ctr = !ctr;
		}
	}

#define FAST_DEV_OP_WRITE	(1)
#define FAST_DEV_OP_READ	(2)
#define FAST_DEV_OP_QUIT	((unsigned long)-1)
	
#define FAST_DEV_HYPERVISOR_TID	0
#define FAST_DEV_GUEST_TID		1

	struct PerGuestData {
		unsigned long fast_device_address;
		unsigned long fast_device_value;
		unsigned long fast_device_size;
		unsigned long fast_device_operation;
		
		captive::lock::barrier fd_hypervisor_barrier;
		captive::lock::barrier fd_guest_barrier;

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

		uint32_t isr;
		uint32_t async_action;		// Pending actions
		uint32_t signal_code;		// Incoming signal code
		uint64_t insns_executed;	// Number of instructions executed
		uint64_t interrupts_taken;
		
		uint32_t execution_mode;	// Mode of execution

		bool verbose_enabled;
	};
}

#endif	/* SHMEM_H */
