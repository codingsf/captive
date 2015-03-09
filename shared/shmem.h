/*
 * File:   shmem.h
 * Author: spink
 *
 * Created on 20 February 2015, 19:11
 */

#ifndef SHMEM_H
#define	SHMEM_H

namespace captive {
	struct shmem_data {
		uint32_t isr;
		uint32_t asynchronous_action_pending;
		bool halt;
		uint64_t insn_count;

		struct cpu_options_t {
			uint32_t mode;
			bool verify;
			uint32_t verify_id;
		} cpu_options;

		struct verify_shm_t {
			uint32_t fail;
			uint8_t barrier_enter[32];
			uint8_t barrier_exit[32];
			uint8_t data[256];
		} *verify_shm_data;

		uint8_t ir_buffer[16384];
	};
}

#endif	/* SHMEM_H */

