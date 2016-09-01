#include <util/command-line.h>
#include <util/cl/option-handler.h>
#include <util/cl/parsing-context.h>

#include <map>

using namespace captive::util;
using namespace captive::util::cl;

std::map<std::string, OptionHandler *> registered_option_handlers;

bool CommandLine::parse(int argc, const char **argv, config::Configuration& cfg)
{
	if (argc < 1) return false;
	
	ParsingContext context(cfg, argc-1, argv+1);
	bool success = true;
	
	while (context.have_tokens()) {
		std::string token = context.consume_token();
		
		if (token.compare(0, 2, "--") == 0) {
			auto ihandler = registered_option_handlers.find(token.substr(2));
			if (ihandler == registered_option_handlers.end()) {
				fprintf(stderr, "error: unrecognised command-line option '%s'\n", token.c_str());
				success = false;
			} else {
				auto handler = ihandler->second;
				
				maybe<std::string> value;
				if (context.have_tokens() && context.peek_token().compare(0, 2, "--") != 0) {
					value = maybe<std::string>(context.consume_token());
				}
				
				if (handler->visited() && handler->option_requirement() == OptionRequirement::Once) {
					fprintf(stderr, "error: command-line option '%s' may only be specified once.\n", token.c_str());
					return false;
				} else {
					handler->visit();
					
					auto result = handler->handle(cfg, value);
					switch (result) {
					case cl::HandleResult::InvalidArgument:
					case cl::HandleResult::MissingArgument:
						success = false;
						fprintf(stderr, "error: command-line option '%s' requires argument\n", token.c_str());
						break;
					}
				}
			}
		} else {
			fprintf(stderr, "error: unhandled command-line argument '%s'\n", token.c_str());
			success = false;
		}
	}
	
	for (auto handler : registered_option_handlers) {
		if (handler.second->option_requirement() == OptionRequirement::Once && !handler.second->visited()) {
			fprintf(stderr, "error: command-line argument '--%s' must be specified\n", handler.first.c_str());
			success = false;
		}
	}
	
	return success;
}

void CommandLine::print_usage()
{
	//
}
