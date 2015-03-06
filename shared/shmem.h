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
		struct {
			uint32_t mode;
			uint32_t verify;
			uint32_t verify_id;
		} options;

		uint32_t isr;
		unsigned int asynchronous_action_pending;
		bool halt;
		uint64_t insn_count;

		struct verify_shm_t {
			uint32_t fail;
			uint8_t barrier_enter[32];
			uint8_t barrier_exit[32];
			uint8_t data[256];
		} *verify_shm_data;

		uint8_t ir_buffer[8192];
	};
}

#endif	/* SHMEM_H */

