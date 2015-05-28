/*
 * File:   malloc.h
 * Author: spink
 *
 * Created on 27 February 2015, 16:44
 */

#ifndef MALLOC_H
#define	MALLOC_H

#include <define.h>
#include <printf.h>

extern void *dlmalloc(size_t);
extern void *dlrealloc(void *p, size_t);
extern void *dlcalloc(size_t, size_t);
extern void dlfree(void *p);

struct mallinfo {
  int arena;    /* non-mmapped space allocated from system */
  int ordblks;  /* number of free chunks */
  int smblks;   /* number of fastbin blocks */
  int hblks;    /* number of mmapped regions */
  int hblkhd;   /* space in mmapped regions */
  int usmblks;  /* maximum total allocated space */
  int fsmblks;  /* space available in freed fastbin blocks */
  int uordblks; /* total allocated space */
  int fordblks; /* total free space */
  int keepcost; /* top-most, releasable (via malloc_trim) space */
};

extern struct mallinfo dlmallinfo(void);

namespace captive {
	struct MemoryVector;

	namespace arch {
		void malloc_init(MemoryVector& arena);

		inline void *malloc(size_t size) { return dlmalloc(size); }

		inline void *realloc(void *p, size_t size) { return dlrealloc(p, size); }

		inline void *calloc(size_t nmemb, size_t size) { return dlcalloc(nmemb, size); }

		inline void dump_mallinfo()
		{
			struct mallinfo mi = dlmallinfo();

			printf("*** MALLOC INFO ***\n");
			printf("Used: %d\n", mi.uordblks);
			printf("Free: %d\n", mi.fordblks);
		}

		inline void free(void *p) { return dlfree(p); }
	}
}

#endif	/* MALLOC_H */

