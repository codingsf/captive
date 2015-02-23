#include <devices/gfx/virtual-screen.h>

using namespace captive::devices::gfx;

VirtualScreen::VirtualScreen() : _config(NULL), _framebuffer(NULL), _palette(NULL)
{

}

VirtualScreen::~VirtualScreen()
{

}

bool VirtualScreen::reset()
{
	if (!reset_configuration())
		return false;

	_config = NULL;
	return true;
}

bool VirtualScreen::configure(const VirtualScreenConfiguration& config)
{
	if (!activate_configuration(config))
		return false;

	_config = &config;
	return true;
}
