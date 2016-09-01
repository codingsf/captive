/**
 * src/net/fd/socket.cpp
 *
 * Protractor :: Copyright (C) University of Edinburgh 2016
 * All Rights Reserved
 */
#include <util/fd/net/socket.h>
#include <util/exception.h>
#include <sys/socket.h>

using namespace captive::util::fd;
using namespace captive::util::fd::net;

Socket::Socket(int fd, AddressFamily::AddressFamily family, SocketType::SocketType type, ProtocolType::ProtocolType protocol, const EndPoint *rep)
	: FileDescriptor(fd),
		_family(family),
		_type(type),
		_protocol(protocol),
		_remote_endpoint(rep)
{

}

/**
 * Constructs a new Socket object with the given parameters.
 * @param family The address family of the Socket.
 * @param type The type of the Socket.
 * @param protocol The protocol for the Socket.
 * @return A Socket object representing a new socket.
 */
Socket *Socket::create(AddressFamily::AddressFamily family, SocketType::SocketType type, ProtocolType::ProtocolType protocol)
{
	// Create the socketfd.
	int fd = ::socket(family, type, protocol);
	if (fd < 0)
		return NULL;

	// Instantiate the Socket wrapper object with the newly created socketfd.
	return new Socket(fd, family, type, protocol, NULL);
}

/**
 * Binds the socket to a particular end-point.
 * @param ep The end-point with which to bind the socket.
 */
void Socket::bind(EndPoint& ep)
{
	// Make sure the end-point family matches the family of the socket.
	if (ep.family() != _family)
		throw Exception("end-point not of the correct family");

	// Obtain the sockaddr from the endpoint.
	socklen_t sa_len;
	struct sockaddr *sa = ep.create_sockaddr(sa_len);
	if (!sa) {
		throw Exception("unable to create sockaddr from endpoint");
	}

	// Bind the socket, and release the sockaddr memory.
	int rc = ::bind(_fd, sa, sa_len);
	ep.free_sockaddr(sa);

	// Check for any errors during the bind.
	if (rc < 0) {
		throw Exception("unable to bind to socket");
	}
}

/**
 * Starts a socket listening for connections.
 * @param max_pending The maximum number of pending connections in the accept queue.
 */
void Socket::listen(int max_pending)
{
	// Instruct the socket to being listening.
	if (::listen(_fd, max_pending) < 0) {
		throw Exception("unable to listen on socket");
	}
}

/**
 * Accepts a new connection pending on this socket, in the form of a new
 * socket object representing the client connection.
 * @return A new socket representing the client connection.
 */
Socket* Socket::accept()
{
	// Allocate an arbitrary amount of storage for the remote socaddr.
	struct sockaddr *sa = (struct sockaddr *)malloc(256);
	socklen_t sa_len = 256;

	// Accept the new socket.
	int new_fd = ::accept(_fd, sa, &sa_len);
	if (new_fd < 0) {
		free(sa);
		return NULL;
	}

	// Create the associated remote end-point.
	const EndPoint *rep = EndPoint::from_sockaddr(sa);
	free(sa);

	// Return a new Socket object that represents the accepted connection.
	return new Socket(new_fd, rep->family(), _type, _protocol, rep);
}
