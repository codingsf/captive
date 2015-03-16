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
		};

		class CommandLine
		{
		public:
			static const CommandLine *parse(int argc, char **argv);

			void dump() const;

		private:
			CommandLine();

			static CommandLineOption *lookup_short_option(char c);
			static CommandLineOption *lookup_long_option(std::string l);

			bool have_unknown;
			std::list<std::string> args;
			std::list<std::string> guest_command_line;
		};
	}
}

#endif	/* COMMAND_LINE_H */

