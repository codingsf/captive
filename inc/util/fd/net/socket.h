/**
 * inc/fd/net/socket.h
 *
 * Protractor :: Copyright (C) University of Edinburgh 2016
 * All Rights Reserved
 */
#ifndef SOCKET_FD_H
#define SOCKET_FD_H

#include <util/fd/fd.h>
#include <util/fd/net/types.h>
#include <util/fd/net/endpoint.h>

namespace captive {
	namespace util {
		namespace fd {
			namespace net {

				class Socket : public FileDescriptor {
				public:
					static Socket *create(AddressFamily::AddressFamily family, SocketType::SocketType type, ProtocolType::ProtocolType protocol);

					void bind(EndPoint& ep);
					void listen(int max_pending);
					Socket *accept();

					const EndPoint *remote_endpoint() const {
						return _remote_endpoint;
					}

				private:
					Socket(int fd, AddressFamily::AddressFamily family, SocketType::SocketType type, ProtocolType::ProtocolType protocol, const EndPoint *rep);

					AddressFamily::AddressFamily _family;
					SocketType::SocketType _type;
					ProtocolType::ProtocolType _protocol;
					const EndPoint *_remote_endpoint;
				};
			}
		}
	}
}

#endif /* SOCKET_FD_H */

