#include <devices/net/network-interface.h>
#include <devices/net/network-device.h>

using namespace captive::devices::net;

void NetworkInterface::attach(NetworkDevice* device)
{
	attached_device = device;
	attached_device->attach_interface(this);
}
