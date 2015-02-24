#include <devices/gfx/virtual-screen.h>

using namespace captive::devices::gfx;

VirtualScreen::VirtualScreen()
	: _config(0, 0, VirtualScreenConfiguration::VS_None),
	_configured(false),
	_framebuffer(NULL),
	_palette(NULL),
	_kbd(NULL),
	_mse(NULL)
{

}

VirtualScreen::~VirtualScreen()
{

}

bool VirtualScreen::reset()
{
	if (!reset_configuration())
		return false;

	_configured = false;
	return true;
}

bool VirtualScreen::configure(const VirtualScreenConfiguration& config)
{
	if (!activate_configuration(config))
		return false;

	_config = config;
	_configured = true;
	return true;
}
