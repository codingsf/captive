#include <devices/io/block/async-block-device.h>
#include <captive.h>

DECLARE_CONTEXT(AsyncBlockDevice)

using namespace captive::devices::io::block;

AsyncBlockDevice::AsyncBlockDevice(uint32_t block_size) : _block_size(block_size)
{
	
}

AsyncBlockDevice::~AsyncBlockDevice()
{

}
