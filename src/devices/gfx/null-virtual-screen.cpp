#include <devices/gfx/null-virtual-screen.h>
#include <captive.h>

using namespace captive::devices::gfx;

NullVirtualScreen::NullVirtualScreen()
{

}

NullVirtualScreen::~NullVirtualScreen()
{

}

bool NullVirtualScreen::initialise()
{
	if (!configured())
		return false;

	DEBUG << "Initialising NULL virtual screen";
	return true;
}

bool NullVirtualScreen::activate_configuration(const VirtualScreenConfiguration& cfg)
{
	DEBUG << "Configuring NULL virtual screen: " << cfg.width() << "x" << cfg.height() << " @ " << cfg.mode() << ", fb=" << std::hex << (uint64_t)framebuffer();
	return true;
}

bool NullVirtualScreen::reset_configuration()
{
	return true;
}
