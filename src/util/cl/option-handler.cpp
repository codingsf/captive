#include <util/cl/option-handler.h>
#include <util/cl/parsing-context.h>
#include <util/config/config.h>

#include <map>

using namespace captive::util;
using namespace captive::util::cl;

extern std::map<std::string, OptionHandler *> registered_option_handlers;

OptionHandlerRegistration::OptionHandlerRegistration(std::string tag, OptionHandler& handler) 
	: _tag(tag), _handler(handler)
{
	registered_option_handlers[tag] = &handler;
}

// -------------------------------------------------------------------------------------------------------------------

DEFINE_OPTION_HANDLER("engine", Engine, OptionRequirement::Once) {
	if (!arg) return HandleResult::MissingArgument;
	config.arch_module = arg.value();
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("kernel", Kernel, OptionRequirement::Once) {
	if (!arg) return HandleResult::MissingArgument;
	config.guest_kernel = arg.value();
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("block-dev-file", BlockDevFile, OptionRequirement::Once) {
	if (!arg) return HandleResult::MissingArgument;
	config.block_device_file = arg.value();
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("tap-device", TAPDevice, OptionRequirement::Once) {
	if (!arg) return HandleResult::MissingArgument;
	config.net_tap_device = arg.value();
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("mac-address", MACAddress, OptionRequirement::Once) {
	if (!arg) return HandleResult::MissingArgument;
	config.net_mac_addr = arg.value();
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("foo", FOO, OptionRequirement::Once) {
	return HandleResult::OK;
}
