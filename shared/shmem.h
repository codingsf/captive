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

	struct VerificationData {
		bool verify_failed;
		uint8_t barrier_enter[32];
		uint8_t barrier_exit[32];
		uint8_t cpu_data[256];
	};

	struct MemoryVector
	{
		void *base_address;
		uint64_t size;
	};

	struct PerGuestData {
		uint64_t next_phys_page;

		MemoryVector shared_memory;
		MemoryVector heap;
		MemoryVector printf_buffer;
	};

	namespace queue {
		struct QueueItem {
			void *data;
			QueueItem *next;
		};

		typedef QueueItem *(*allocate_fn_t)();
		typedef void (*free_fn_t)(QueueItem *);

		inline void enqueue(QueueItem **queue, QueueItem *new_item)
		{
			while (*queue) {
				queue = &((*queue)->next);
			}

			new_item->next = NULL;

			(*queue) = new_item;
		}

		inline QueueItem *dequeue(QueueItem **queue)
		{
			QueueItem *head = *queue;
			if (!head) return NULL;

			*queue = head->next;
			return head;
		}
	}

	struct PerCPUData {
		PerGuestData *guest_data;
		volatile VerificationData *verify_data;

		bool halt;

		uint32_t isr;			// Interrupt Status Register
		uint32_t async_action;		// Pending actions
		uint32_t signal_code;		// Incoming signal code
		uint64_t insns_executed;	// Number of instructions executed
		uint64_t interrupts_taken;
		
		uint32_t execution_mode;	// Mode of execution
		uint32_t entrypoint;		// Entrypoint of the guest

		bool verbose_enabled;
		bool verify_enabled;
		uint32_t verify_tid;

		uint32_t device_address;

		lock::SpinLock rwu_ready_queue_lock;
		queue::QueueItem *rwu_ready_queue;
	};
}

#endif	/* SHMEM_H */

