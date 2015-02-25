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
	namespace logging {
		class LogContext
		{
		public:
			explicit LogContext(std::string name) : _parent(NULL), _name(name) { }

			inline std::string name() const { return _name; }
		private:
			LogContext *_parent;
			std::string _name;
		};
	}

	class LogStream : public std::stringstream
	{
	public:
		explicit LogStream() : _ctx(NULL), _level(LL_DEBUG)
		{
		}

		~LogStream()
		{
			std::cerr << level_text(_level) << ": " << (_ctx != NULL ? _ctx->name() : "?") << ": " << this->str() << std::endl;
		}

		enum log_level {
			LL_DEBUG,
			LL_INFO,
			LL_WARNING,
			LL_ERROR,
		};

		inline void context(const logging::LogContext& ctx)
		{
			_ctx = &ctx;
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
		const logging::LogContext *_ctx;
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
		const logging::LogContext& ctx;
	};

	inline __set_context set_context(const logging::LogContext& ctx)
	{
		return { ctx };
	}

	template<typename _CharT, typename _Traits>
	inline std::basic_ostream<_CharT, _Traits>&
	operator<<(std::basic_ostream<_CharT, _Traits>& __os, __set_context __f)
	{
		((LogStream&)__os).context(__f.ctx);
		return __os;
	}
}

#define LOG() captive::LogStream()
#define DEBUG LOG() << captive::set_level(captive::LogStream::LL_DEBUG)
#define INFO LOG() << captive::set_level(captive::LogStream::LL_INFO)
#define ERROR LOG() << captive::set_level(captive::LogStream::LL_ERROR)
#define WARNING LOG() << captive::set_level(captive::LogStream::LL_WARNING)

#define CONTEXT(_ctx) captive::set_context(captive::logging::LogContext##_ctx)

#define DECLARE_CONTEXT(_ctx) namespace captive { namespace logging { LogContext LogContext##_ctx(#_ctx); } }
#define DECLARE_CHILD_CONTEXT(_child, _parent) DECLARE_CONTEXT(_child)
#define USE_CONTEXT(_ctx) namespace captive { namespace logging { extern LogContext LogContext##_ctx; } }

#define LAST_ERROR_TEXT strerror(errno)

#endif	/* CAPTIVE_H */

