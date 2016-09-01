/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   exception.h
 * Author: s0457958
 *
 * Created on 01 September 2016, 09:47
 */

#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <string>

namespace captive {
	namespace util {

		class Exception {
		public:

			Exception(std::string message) : _message(message) {
			}

			std::string message() const {
				return _message;
			}

		private:
			std::string _message;
		};

		class NotImplementedException : public Exception {
		public:

			NotImplementedException() : Exception("not implemented") {
			}
		};

	}
}

#endif /* EXCEPTION_H */

