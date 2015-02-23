#include <devices/io/virtio/virtio-block-device.h>
#include <devices/io/block-device.h>
#include <captive.h>

#include <string.h>

using namespace captive::devices::io::virtio;

VirtIOBlockDevice::VirtIOBlockDevice(irq::IRQLine& irq, BlockDevice& bdev) : VirtIO(irq, 1, 2, 1), _bdev(bdev)
{
	bzero(&config, sizeof(config));
	config.capacity = bdev.blocks();
	config.block_size = bdev.block_size();

	host_features = 1 << 6;
}

VirtIOBlockDevice::~VirtIOBlockDevice()
{

}

void VirtIOBlockDevice::process_event(VirtIOQueueEvent& evt)
{
	//DEBUG << "Processing Event";

	if (evt.read_buffers.size() == 0 && evt.write_buffers.size() == 0) {
		return;
	}

	if (evt.read_buffers.front().size < sizeof(struct virtio_blk_req)) {
		WARNING << "Discarding event with invalid header";
		return;
	}

	struct virtio_blk_req *req = (struct virtio_blk_req *)evt.read_buffers.front().data;

	uint8_t *status = (uint8_t *)evt.write_buffers.back().data;
	switch (req->type) {
	case 0: // Read
		assert(evt.write_buffers.size() == 2);

		if (handle_read(req->sector, (uint8_t *)evt.write_buffers.front().data, evt.write_buffers.front().size)) {
			*status = 0;
		} else {
			*status = 1;
		}

		evt.response_size = evt.write_buffers.front().size + 1;
		break;

	case 1: // Write
		assert(evt.read_buffers.size() == 2);

		if (handle_write(req->sector, (uint8_t *)evt.read_buffers.back().data, evt.read_buffers.back().size)) {
			*status = 0;
		} else {
			*status = 1;
		}

		evt.response_size = 1;
		break;
	case 8: // Get ID
	{
		assert(evt.write_buffers.size() == 2);

		char *serial_number = (char *)evt.write_buffers.front().data;
		strncpy(serial_number, "virtio", evt.write_buffers.front().size);

		evt.response_size = 1 + 7;
		*status = 0;

		break;
	}
	default:
		WARNING << "Rejecting event with unsupported type " << (uint32_t)req->type;

		*status = 2;
		evt.response_size = 1;
		break;
	}

	assert_interrupt(1);
}

void VirtIOBlockDevice::reset()
{

}

bool VirtIOBlockDevice::handle_read(uint64_t sector, uint8_t* buffer, uint32_t len)
{
	return _bdev.read_blocks(sector, len / _bdev.block_size(), buffer);
}

bool VirtIOBlockDevice::handle_write(uint64_t sector, uint8_t* buffer, uint32_t len)
{
	return _bdev.write_blocks(sector, len / _bdev.block_size(), buffer);
}
