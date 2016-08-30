#include <util/command-line.h>
#include <string.h>

#define CL_DEFINE_OPTIONS
#include <util/cl/options.h>
#undef CL_DEFINE_OPTIONS

using namespace captive::util;

#define NR_COMMAND_LINE_OPTIONS (sizeof(cl::command_line_options) / sizeof(cl::command_line_options[0]))

CommandLine::CommandLine() : _have_unknown(false), _have_missing(false)
{

}

const CommandLine *CommandLine::parse(int argc, char** argv)
{
	CommandLine *cl = new CommandLine();
	bool readGuestCommandLine = false;

	for (int i = 0; i < argc; i++) {
		if (readGuestCommandLine) {
			cl->guest_command_line.push_back(std::string(argv[i]));
		} else {
			if (argv[i][0] == '-') {
				if (argv[i][1] == '-') {
					if (argv[i][2] == '\0') {
						readGuestCommandLine = true;
					} else {
						CommandLineOption *opt = lookup_long_option(std::string(&argv[i][2]));
						if (opt) {
							opt->present = true;
							
							if (opt->option_value == CommandLineOption::Optional) {
								if (i == (argc - 1)) {
									continue;
								} else {
									if (argv[i+1][0] == '-')
										continue;
									
									i++;
									opt->value = std::string(argv[i]);
								}
							} else if (opt->option_value == CommandLineOption::Required) {
								if (i == (argc - 1)) {
									cl->_have_missing = true;
								} else {
									if (argv[i+1][0] == '-')
										cl->_have_missing = true;
									
									i++;
									opt->value = std::string(argv[i]);
								}
							}
							
						} else {
							cl->_have_unknown = true;
						}
					}
				} else {
					for (unsigned int n = 1; n < strlen(argv[i]); n++) {
						CommandLineOption *opt = lookup_short_option(argv[i][n]);
						if (opt) {
							opt->present = true;
						} else {
							cl->_have_unknown = true;
						}
					}
				}
			} else {
				cl->args.push_back(std::string(argv[i]));
			}
		}
	}

	return cl;
}

CommandLineOption *CommandLine::lookup_short_option(char c)
{
	for (unsigned int i = 0; i < NR_COMMAND_LINE_OPTIONS; i++) {
		if (cl::command_line_options[i]->short_name == c) {
			return cl::command_line_options[i];
		}
	}

	return NULL;
}

CommandLineOption *CommandLine::lookup_long_option(std::string l)
{
	for (unsigned int i = 0; i < NR_COMMAND_LINE_OPTIONS; i++) {
		if (cl::command_line_options[i]->long_name == l) {
			return cl::command_line_options[i];
		}
	}

	return NULL;
}

void CommandLine::dump() const
{

}

void CommandLine::print_usage() const
{
	fprintf(stderr, "command-line options:\n");
	
	for (unsigned int i = 0; i < NR_COMMAND_LINE_OPTIONS; i++) {
		auto opt = cl::command_line_options[i];
		
		if (opt->short_name.has_value()) {
			fprintf(stderr, " -%c  ", opt->short_name.value());
		} else {
			fprintf(stderr, "      ");
		}
		
		fprintf(stderr, "--%s", opt->long_name.c_str());
		
		if (opt->option_value == CommandLineOption::Optional) {
			fprintf(stderr, " [value]\n");
		} else if (opt->option_value == CommandLineOption::Required) {
			fprintf(stderr, " <value>\n");
		} else {
			fprintf(stderr, "\n");
		}
	}
}
