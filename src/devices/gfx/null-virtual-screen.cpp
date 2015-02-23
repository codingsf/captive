#include <devices/gfx/null-virtual-screen.h>

using namespace captive::devices::gfx;

NullVirtualScreen::NullVirtualScreen()
{

}

NullVirtualScreen::~NullVirtualScreen()
{

}

bool NullVirtualScreen::initialise()
{
	return true;
}

bool NullVirtualScreen::activate_configuration(const VirtualScreenConfiguration& cfg)
{
	return true;
}

bool NullVirtualScreen::reset_configuration()
{
	return true;
}
