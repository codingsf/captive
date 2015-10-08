#include <socket.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace captive;

Socket::Socket() : fd(-1)
{
}

Socket::Socket(int fd) : fd(fd)
{
}

Socket::~Socket()
{
	close();
}

void Socket::close() {
	if (fd >= 0) ::close(fd);
	fd = -1;
}

ServerSocket::ServerSocket() { }

ServerSocket::~ServerSocket()
{
}

ClientSocket::ClientSocket() { }
ClientSocket::~ClientSocket() { }

void UnixDomainSocket::listen(int max_pending)
{
	if (fd < 0) return;
	::listen(fd, max_pending);
}

Socket *UnixDomainSocket::accept()
{
	int clientfd = ::accept(fd, NULL, NULL);
	return new Socket(clientfd);
}

UnixDomainSocket::UnixDomainSocket(std::string path) : path(path)
{
}

UnixDomainSocket::~UnixDomainSocket() {

}


bool UnixDomainSocket::create()
{
	fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
	return fd >= 0;
}

bool UnixDomainSocket::bind()
{
	struct sockaddr_un sockaddr;
	
	sockaddr.sun_family = AF_UNIX;
	strcpy(sockaddr.sun_path, path.c_str());
	
	int len = strlen(sockaddr.sun_path) + sizeof(sockaddr.sun_family);
	return ::bind(fd, (struct sockaddr *)&sockaddr, len) == 0;
}

bool UnixDomainSocket::connect()
{
	struct sockaddr_un sockaddr;
	
	sockaddr.sun_family = AF_UNIX;
	strcpy(sockaddr.sun_path, path.c_str());
	
	int len = strlen(sockaddr.sun_path) + sizeof(sockaddr.sun_family);
	return ::connect(fd, (struct sockaddr *)&sockaddr, len) == 0;
}

void TCPSocket::listen(int max_pending)
{
	if (fd < 0) return;
	::listen(fd, max_pending);
}

Socket *TCPSocket::accept()
{
	int clientfd = ::accept(fd, NULL, NULL);
	return new Socket(clientfd);
}

TCPSocket::TCPSocket(std::string host, int port) : host(host), port(port)
{
}

TCPSocket::~TCPSocket() {

}


bool TCPSocket::create()
{
	fd = ::socket(AF_INET, SOCK_STREAM, 0);
	return fd >= 0;
}

bool TCPSocket::bind()
{
	struct sockaddr_in sockaddr;
	
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = INADDR_ANY;
	sockaddr.sin_port = htons(port);
	
	int len = sizeof(sockaddr);
	return ::bind(fd, (struct sockaddr *)&sockaddr, len) == 0;
}

bool TCPSocket::connect()
{
	return false;
}
