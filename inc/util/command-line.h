/*
 * File:   command-line.h
 * Author: spink
 *
 * Created on 13 March 2015, 22:06
 */

#ifndef COMMAND_LINE_H
#define	COMMAND_LINE_H

#include <list>
#include <map>
#include <util/maybe.h>

namespace captive {
	namespace util {
		class CommandLineOption
		{
		public:
			enum OptionValue
			{
				None,
				Optional,
				Required
			};

			CommandLineOption(char short_name, std::string long_name, std::string description, OptionValue option_value)
				: short_name(short_name), long_name(long_name), description(description), option_value(option_value)
			{

			}

			maybe<char> short_name;
			std::string long_name;
			std::string description;
			OptionValue option_value;

			bool present;
			maybe<std::string> value;

			inline operator bool() {
				return present;
			}
			
			inline std::string get() const {
				return value.value();
			}
		};

		class CommandLine
		{
		public:
			static const CommandLine *parse(int argc, char **argv);

			void dump() const;
			void print_usage() const;
			
			inline bool have_unknown() const { return _have_unknown; }
			inline bool have_missing() const { return _have_missing; }
			inline bool have_errors() const { return have_unknown() || have_missing(); }

		private:
			CommandLine();

			static CommandLineOption *lookup_short_option(char c);
			static CommandLineOption *lookup_long_option(std::string l);

			bool _have_unknown;
			bool _have_missing;
			
			std::list<std::string> args;
			std::list<std::string> guest_command_line;
		};
	}
}

#endif	/* COMMAND_LINE_H */

