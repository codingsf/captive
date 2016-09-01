/**
 * inc/fd/event.h
 *
 * Protractor :: Copyright (C) University of Edinburgh 2016
 * All Rights Reserved
 */
#ifndef EVENT_H
#define EVENT_H

#include <util/fd/fd.h>

namespace captive {
	namespace util {
		namespace fd {

			class Event : public FileDescriptor {
			public:
				static Event *create();

				void invoke();

			private:
				Event(int fd);
			};
		}
	}
}

#endif /* EVENT_H */

