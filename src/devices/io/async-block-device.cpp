#include <devices/io/async-block-device.h>

using namespace captive::devices::io;

AsyncBlockDevice::AsyncBlockDevice(uint32_t block_size) : _block_size(block_size)
{
	
}

AsyncBlockDevice::~AsyncBlockDevice()
{

}
