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
		namespace config {
			class Configuration;
		}
		
		class CommandLine
		{
		public:
			static bool parse(int argc, const char **argv, config::Configuration& cfg);
			static void print_usage();
			
		private:
			CommandLine();
		};
	}
}

#endif	/* COMMAND_LINE_H */
