/**
 * inc/fd/net/endpoint.h
 *
 * Protractor :: Copyright (C) University of Edinburgh 2016
 * All Rights Reserved
 */
#ifndef ENDPOINT_H
#define ENDPOINT_H

#include <util/fd/net/types.h>
#include <sys/socket.h>

namespace captive {
	namespace util {
		namespace fd {
			namespace net {

				class EndPoint {
				public:
					EndPoint(AddressFamily::AddressFamily family);
					virtual ~EndPoint();

					AddressFamily::AddressFamily family() const {
						return _family;
					}

					virtual struct sockaddr *create_sockaddr(socklen_t& len) = 0;
					virtual void free_sockaddr(struct sockaddr *sa) = 0;

					static const EndPoint *from_sockaddr(const struct sockaddr *sa);

				private:
					AddressFamily::AddressFamily _family;
				};
			}
		}
	}
}

#endif /* ENDPOINT_H */

