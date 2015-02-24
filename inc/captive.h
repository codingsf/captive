/*
 * File:   captive.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 14:28
 */

#ifndef CAPTIVE_H
#define	CAPTIVE_H

#include <define.h>
#include <iostream>
#include <sstream>
#include <errno.h>
#include <string.h>

namespace captive {
	class LogStream : public std::stringstream
	{
	public:
		explicit LogStream() { }
		~LogStream() { std::cerr << this->str() << std::endl; }
	};
}

#define LOG() captive::LogStream()
#define DEBUG LOG() << "debug: "
#define ERROR LOG() << "error: "
#define WARNING LOG() << "warning: "
#define LAST_ERROR_TEXT strerror(errno)
#define CONTEXT(a) #a ": "
#define WHERE(a) ""

#endif	/* CAPTIVE_H */

