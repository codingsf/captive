/* 
 * File:   kernel-loader.h
 * Author: s0457958
 *
 * Created on 05 August 2015, 10:49
 */

#ifndef KERNEL_LOADER_H
#define	KERNEL_LOADER_H

#include <define.h>
#include <loader/loader.h>

namespace captive {
	namespace loader {
		class KernelLoader : public Loader
		{
		public:
			virtual gpa_t entrypoint() const = 0;
			virtual bool requires_device_tree() const = 0;
			
			static KernelLoader *create_from_file(std::string filename);
		};
	}
}

#endif	/* KERNEL_LOADER_H */

