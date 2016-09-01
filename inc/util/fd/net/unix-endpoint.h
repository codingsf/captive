/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   unix-endpoint.h
 * Author: s0457958
 *
 * Created on 01 September 2016, 09:50
 */

#ifndef UNIX_ENDPOINT_H
#define UNIX_ENDPOINT_H

#include <util/fd/net/endpoint.h>
#include <string>

namespace captive {
	namespace util {
		namespace fd {
			namespace net {

				class UnixEndPoint : public EndPoint {
				public:
					UnixEndPoint(std::string path);
					~UnixEndPoint();

					const std::string& path() const {
						return _path;
					}
					
					virtual struct sockaddr *create_sockaddr(socklen_t& len) override;
					virtual void free_sockaddr(struct sockaddr *sa) override;

				private:
					const std::string _path;
				};
			}
		}
	}
}

#endif /* UNIX_ENDPOINT_H */

