/*
 * File:   gpa-resolver.h
 * Author: spink
 *
 * Created on 23 February 2015, 09:02
 */

#ifndef GPA_RESOLVER_H
#define	GPA_RESOLVER_H

#include <define.h>

namespace captive {
	namespace hypervisor {
		class GPAResolver {
		public:
			bool resolve_gpa(gpa_t addr, void *& out_addr) const;
		};
	}
}

#endif	/* GPA_RESOLVER_H */

