/*
 * File:   options.h
 * Author: spink
 *
 * Created on 16 March 2015, 08:36
 */

#ifndef OPTIONS_H
#define	OPTIONS_H

#include <util/command-line.h>

namespace captive {
	namespace util {
		namespace cl {

#ifdef CL_DEFINE_OPTIONS

// Build the option class definitions
#define DefineFlag(_name, _short, _long, _desc) CommandLineOption _name(_short, _long, _desc, CommandLineOption::None);
#define DefineValueRequired(_name, _short, _long, _desc) CommandLineOption _name(_short, _long, _desc, CommandLineOption::Required);
#define DefineValueOptional(_name, _short, _long, _desc) CommandLineOption _name(_short, _long, _desc, CommandLineOption::Optional);
#include <util/cl/option-definitions.h>
#undef DefineValueOptional
#undef DefineValueRequired
#undef DefineFlag

// Build the option list
#define DefineFlag(_name, _short, _long, _desc) &_name,
#define DefineValueRequired(_name, _short, _long, _desc) &_name,
#define DefineValueOptional(_name, _short, _long, _desc) &_name,

static CommandLineOption *command_line_options[] = {
#include <util/cl/option-definitions.h>
};

#undef DefineValueOptional
#undef DefineValueRequired
#undef DefineFlag

#else

// Build the extern option class definitions
#define DefineFlag(_name, _short, _long, _desc) extern CommandLineOption _name;
#define DefineValueRequired(_name, _short, _long, _desc) extern CommandLineOption _name;
#define DefineValueOptional(_name, _short, _long, _desc) extern CommandLineOption _name;
#include <util/cl/option-definitions.h>
#undef DefineValueOptional
#undef DefineValueRequired
#undef DefineFlag
#endif

		}
	}
}

#endif	/* OPTIONS_H */

