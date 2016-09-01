/**
 * inc/fd/fd.h
 *
 * Protractor :: Copyright (C) University of Edinburgh 2016
 * All Rights Reserved
 */
#ifndef FD_H
#define FD_H

#include <define.h>

namespace captive {
	namespace util {
		namespace fd {

			class FileDescriptor {
			public:
				~FileDescriptor();
				void close();

				int fd() const {
					return _fd;
				}

				int read(void *buffer, size_t size);
				int write(void *buffer, size_t size);

			protected:
				FileDescriptor(int fd);
				int _fd;
			};
		}
	}
}

#endif /* FD_H */
