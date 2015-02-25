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

	class LogStream : public std::stringstream {
	public:
		explicit LogStream() : _context_id("?")
		{
		}

		~LogStream()
		{
			std::cerr << level_text(_level) << ": " << _context_id << ": " << this->str() << std::endl;
		}

		enum log_level {
			LL_DEBUG,
			LL_INFO,
			LL_WARNING,
			LL_ERROR,
		};

		inline void context(std::string id)
		{
			_context_id = id;
		}

		inline void level(log_level new_level)
		{
			_level = new_level;
		}

		inline std::string level_text(log_level lvl) const {
			switch (lvl) {
			case LL_DEBUG:
				return "debug";
			case LL_INFO:
				return "info";
			case LL_WARNING:
				return "warning";
			case LL_ERROR:
				return "error";
			default:
				return "?";
			}
		}

	private:
		std::string _context_id;
		log_level _level;
	};

	struct __set_level {
		LogStream::log_level level;
	};

	inline __set_level set_level(LogStream::log_level level)
	{
		return { level };
	}

	template<typename _CharT, typename _Traits>
	inline std::basic_ostream<_CharT, _Traits>&
	operator<<(std::basic_ostream<_CharT, _Traits>& __os, __set_level __f)
	{
		((LogStream&)__os).level(__f.level);
		return __os;
	}

	struct __set_context {
		std::string context_id;
	};

	inline __set_context set_context(std::string id)
	{
		return { id };
	}

	template<typename _CharT, typename _Traits>
	inline std::basic_ostream<_CharT, _Traits>&
	operator<<(std::basic_ostream<_CharT, _Traits>& __os, __set_context __f)
	{
		((LogStream&)__os).context(__f.context_id);
		return __os;
	}
}

#define LOG() captive::LogStream()
#define DEBUG LOG() << captive::set_level(captive::LogStream::LL_DEBUG)
#define INFO LOG() << captive::set_level(captive::LogStream::LL_INFO)
#define ERROR LOG() << captive::set_level(captive::LogStream::LL_ERROR)
#define WARNING LOG() << captive::set_level(captive::LogStream::LL_WARNING)

#define CONTEXT(a) captive::set_context(#a)

#define LAST_ERROR_TEXT strerror(errno)

#endif	/* CAPTIVE_H */

