/*
 * File:   shmem.h
 * Author: spink
 *
 * Created on 20 February 2015, 19:11
 */

#ifndef SHMEM_H
#define	SHMEM_H

namespace captive {
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

	struct PerCPUData {
		PerGuestData *guest_data;
		VerificationData *verify_data;

		bool halt;

		uint32_t isr;			// Interrupt Status Register
		uint32_t async_action;		// Pending actions
		uint32_t signal_code;		// Incoming signal code
		uint64_t insns_executed;	// Number of instructions executed

		uint32_t execution_mode;	// Mode of execution
		uint32_t entrypoint;		// Entrypoint of the guest

		bool verify_enabled;
		uint32_t verify_tid;

		void *rwu[4];
	};
}

#endif	/* SHMEM_H */

