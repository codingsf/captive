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

DEFINE_OPTION_HANDLER("engine", Engine, OptionRequirement::Once, ValueRequirement::Required) {
	config.arch_module = arg;
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("kernel", Kernel, OptionRequirement::Once, ValueRequirement::Required) {
	config.guest_kernel = arg;
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("block-dev-file", BlockDevFile, OptionRequirement::Once, ValueRequirement::Required) {
	config.block_device_file = arg;
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("tap-device", TAPDevice, OptionRequirement::None, ValueRequirement::Required) {
	config.net_tap_device = arg;
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("mac-address", MACAddress, OptionRequirement::None, ValueRequirement::Required) {
	config.net_mac_addr = arg;
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("help", Help, OptionRequirement::None, ValueRequirement::None) {
	config.print_usage = true;
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("cache-sim", CacheSimulation, OptionRequirement::None, ValueRequirement::None) {
	config.cache_simulation = true;
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("icount", CountInstructions, OptionRequirement::None, ValueRequirement::None) {
	config.insn_count = true;
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("platform", Platform, OptionRequirement::Once, ValueRequirement::Required) {
	config.platform = arg;
	return HandleResult::OK;
}

DEFINE_OPTION_HANDLER("device-tree", DeviceTree, OptionRequirement::None, ValueRequirement::Required) {
	config.device_tree = arg;
	return HandleResult::OK;
}
