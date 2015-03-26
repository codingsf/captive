/*
 * File:   guest-shared-memory.h
 * Author: spink
 *
 * Created on 26 March 2015, 11:22
 */

#ifndef GUEST_SHARED_MEMORY_H
#define	GUEST_SHARED_MEMORY_H

namespace captive {
	namespace hypervisor {
		class SharedMemory
		{
		public:
			virtual void *allocate(size_t size) = 0;
			virtual void free(void *p) = 0;
		};
	}
}

#endif	/* GUEST_SHARED_MEMORY_H */

