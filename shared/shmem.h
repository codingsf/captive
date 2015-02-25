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
		unsigned int asynchronous_action_pending;
		bool halt;
		uint64_t insn_count;
	};
}

#endif	/* SHMEM_H */

