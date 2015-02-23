#include <devices/gfx/virtual-screen.h>

using namespace captive::devices::gfx;

VirtualScreen::VirtualScreen() : _configured(false), _framebuffer(NULL), _palette(NULL)
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

	_configured = true;
	return true;
}
