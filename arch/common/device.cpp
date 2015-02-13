#include <device.h>

using namespace captive::arch;

Device::Device(Environment& env) : _env(env)
{

}

Device::~Device()
{

}

CoreDevice::CoreDevice(Environment& env) : Device(env)
{

}

CoreDevice::~CoreDevice()
{

}
