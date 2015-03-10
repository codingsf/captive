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

	struct PerGuestData {
		uint64_t next_phys_page;

		void *heap;
		uint64_t heap_size;

		void *ir_buffer;
		uint64_t ir_buffer_size;

		void *code_buffer;
		uint64_t code_buffer_size;

		void *printf_buffer;
		uint64_t printf_buffer_size;
	};

	struct PerCPUData {
		PerGuestData *guest_data;
		VerificationData *verify_data;

		bool halt;

		uint32_t isr;			// Interrupt Status Register
		uint32_t async_action;		// Pending actions
		uint64_t insns_executed;	// Number of instructions executed

		uint32_t execution_mode;	// Mode of execution
		uint32_t entrypoint;		// Entrypoint of the guest

		bool verify_enabled;
		uint32_t verify_tid;
	};
}

#endif	/* SHMEM_H */

