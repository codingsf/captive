/**
 * inc/net/fd/ip-endpoint.h
 *
 * Protractor :: Copyright (C) University of Edinburgh 2016
 * All Rights Reserved
 */
#ifndef IP_ENDPOINT_H
#define IP_ENDPOINT_H

#include <util/fd/net/endpoint.h>
#include <util/fd/net/ip-address.h>

namespace captive {
	namespace util {
		namespace fd {
			namespace net {

				class IPEndPoint : public EndPoint {
				public:
					IPEndPoint(const IPAddress& addr, int port);
					~IPEndPoint();

					const IPAddress& address() const {
						return _addr;
					}

					int port() const {
						return _port;
					}

					virtual struct sockaddr *create_sockaddr(socklen_t& len) override;
					virtual void free_sockaddr(struct sockaddr *sa) override;

				private:
					const IPAddress _addr;
					int _port;
				};
			}
		}
	}
}

#endif /* IP_ENDPOINT_H */

