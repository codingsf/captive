#include <devices/io/virtio/virtio-block-device.h>
#include <devices/io/async-block-device.h>
#include <captive.h>

#include <string.h>

USE_CONTEXT(VirtIO);
DECLARE_CHILD_CONTEXT(VirtIOBlockDevice, VirtIO);

using namespace captive::devices::io::virtio;

VirtIOBlockDevice::VirtIOBlockDevice(irq::IRQLine& irq, AsyncBlockDevice& bdev) : VirtIO(irq, 1, 2, 1), _bdev(bdev)
{
	bzero(&config, sizeof(config));
	config.capacity = bdev.blocks();
	config.block_size = bdev.block_size();

	set_host_feature(6);
}

VirtIOBlockDevice::~VirtIOBlockDevice()
{

}

void VirtIOBlockDevice::process_event(VirtIOQueueEvent& evt)
{
	//DEBUG << CONTEXT(VirtIOBlockDevice) << "Processing Event";

	if (evt.read_buffers.size() == 0 && evt.write_buffers.size() == 0) {
		return;
	}

	if (evt.read_buffers.front().size < sizeof(struct virtio_blk_req)) {
		WARNING << CONTEXT(VirtIOBlockDevice) << "Discarding event with invalid header";
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
		WARNING << CONTEXT(VirtIOBlockDevice) << "Rejecting event with unsupported type " << (uint32_t)req->type;

		*status = 2;
		evt.response_size = 1;
		break;
	}

	assert_interrupt(0);
}

void VirtIOBlockDevice::reset()
{

}

bool VirtIOBlockDevice::handle_read(uint64_t sector, uint8_t* buffer, uint32_t len)
{
	//DEBUG << CONTEXT(VirtIOBlockDevice) << "Handling Read: sector=" << std::hex << sector << ", len=" << len << ", buffer=" << std::hex << (uint64_t)buffer;
	
	AsyncBlockRequest *rq = new AsyncBlockRequest();
	rq->block_count = len / _bdev.block_size();
	rq->block_offset = sector;
	rq->buffer = buffer;
	rq->is_read = true;
	
	if (!_bdev.submit_request(rq, NULL)) return false;
	
	bool result = rq->complete.wait();
	delete rq;
	
	return result;
}

bool VirtIOBlockDevice::handle_write(uint64_t sector, uint8_t* buffer, uint32_t len)
{
	//DEBUG << CONTEXT(VirtIOBlockDevice) << "Handling Write: sector=" << std::hex << sector << ", len=" << len << ", buffer=" << std::hex << (uint64_t)buffer;
	AsyncBlockRequest *rq = new AsyncBlockRequest();
	rq->block_count = len / _bdev.block_size();
	rq->block_offset = sector;
	rq->buffer = buffer;
	rq->is_read = false;
	
	if (!_bdev.submit_request(rq, NULL)) return false;
	
	bool result = rq->complete.wait();
	delete rq;
	
	return result;
}
