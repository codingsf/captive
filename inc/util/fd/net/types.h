/**
 * inc/fd/net/types.h
 *
 * Protractor :: Copyright (C) University of Edinburgh 2016
 * All Rights Reserved
 */
#ifndef TYPES_H
#define TYPES_H

namespace captive {
	namespace util {
		namespace fd {
			namespace net {
				namespace AddressFamily {

					enum AddressFamily {
						Unix = 1,
						IPv4 = 2,
						IPv6 = 10,
						None = 255
					};
				}

				namespace SocketType {

					enum SocketType {
						Stream = 1,
						Datagram = 2,
						Raw = 3,
					};
				}

				namespace ProtocolType {

					enum ProtocolType {
						None = 0,
					};
				}
			}
		}
	}
}

#endif /* TYPES_H */

