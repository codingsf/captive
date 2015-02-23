#include <devices/io/block-device.h>

using namespace captive::devices::io;

BlockDevice::BlockDevice(uint32_t block_size) : _block_size(block_size)
{

}

BlockDevice::~BlockDevice()
{

}
