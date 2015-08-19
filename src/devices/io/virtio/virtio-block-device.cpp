#include <devices/io/virtio/virtio-block-device.h>
#include <devices/io/virtio/virtqueue.h>
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

void VirtIOBlockDevice::process_event(VirtIOQueueEvent *evt)
{
	//DEBUG << CONTEXT(VirtIOBlockDevice) << "Processing Event";

	if (evt->read_buffers.size() == 0 && evt->write_buffers.size() == 0) {
		return;
	}

	if (evt->read_buffers.front().size < sizeof(struct virtio_blk_req)) {
		WARNING << CONTEXT(VirtIOBlockDevice) << "Discarding event with invalid header";
		return;
	}

	struct virtio_blk_req *req = (struct virtio_blk_req *)evt->read_buffers.front().data;

	uint8_t *status = (uint8_t *)evt->write_buffers.back().data;
	switch (req->type) {
	case 0: // Read
		handle_read_request(evt, req);
		return;	// We return here, because of asychronicity.

	case 1: // Write
		handle_write_request(evt, req);
		return;	// We return here, because of asychronicity.
		
	case 8: // Get ID
	{
		assert(evt->write_buffers.size() == 2);

		char *serial_number = (char *)evt->write_buffers.front().data;
		strncpy(serial_number, "virtio", evt->write_buffers.front().size);

		evt->response_size = 1 + 7;
		*status = 0;

		break;
	}
	
	default:
		WARNING << CONTEXT(VirtIOBlockDevice) << "Rejecting event with unsupported type " << (uint32_t)req->type;

		*status = 2;
		evt->response_size = 1;
		break;
	}

	assert_interrupt(0);
	evt->queue.Push(evt->head_idx, evt->response_size);
}

void VirtIOBlockDevice::reset()
{

}

static void handle_read_request_callback(captive::devices::io::AsyncBlockRequest *rq)
{
	VirtIOQueueEvent *evt = (VirtIOQueueEvent *)rq->opaque1;
	VirtIOBlockDevice *bdev = (VirtIOBlockDevice *)rq->opaque2;
	
	uint8_t *status = (uint8_t *)evt->write_buffers.back().data;
	
	if (rq->request_complete.wait()) {
		*status = 0;
	} else {
		*status = 1;
	}
	
	evt->response_size = evt->write_buffers.front().size + 1;

	DEBUG << "Block device read request completed";
	evt->queue.Push(evt->head_idx, evt->response_size);
	bdev->signal_completion();
	
	delete rq;
}

static void handle_write_request_callback(captive::devices::io::AsyncBlockRequest *rq)
{
	VirtIOQueueEvent *evt = (VirtIOQueueEvent *)rq->opaque1;
	VirtIOBlockDevice *bdev = (VirtIOBlockDevice *)rq->opaque2;
	
	uint8_t *status = (uint8_t *)evt->write_buffers.back().data;
	
	if (rq->request_complete.wait()) {
		*status = 0;
	} else {
		*status = 1;
	}
	
	evt->response_size = 1;

	evt->queue.Push(evt->head_idx, evt->response_size);
	bdev->signal_completion();
	
	delete rq;
}

void VirtIOBlockDevice::handle_read_request(VirtIOQueueEvent *evt, struct virtio_blk_req *req)
{
	assert(evt->write_buffers.size() == 2);
	
	VirtIOQueueEventBuffer& evt_buffer = evt->write_buffers.front();
	
	AsyncBlockRequest *rq = new AsyncBlockRequest();
	rq->block_count = evt_buffer.size / _bdev.block_size();
	rq->buffer = (uint8_t *)evt_buffer.data;
	rq->is_read = true;
	rq->offset = req->sector * _bdev.block_size();
	rq->opaque1 = evt;
	rq->opaque2 = this;
	
	if (!_bdev.submit_request(rq, handle_read_request_callback)) {
		ERROR << "Block device read request submission failed";
	}
}

void VirtIOBlockDevice::handle_write_request(VirtIOQueueEvent *evt, struct virtio_blk_req *req)
{
	assert(evt->read_buffers.size() == 2);
	
	VirtIOQueueEventBuffer& evt_buffer = evt->read_buffers.back();
	
	AsyncBlockRequest *rq = new AsyncBlockRequest();
	rq->block_count = evt_buffer.size / _bdev.block_size();
	rq->buffer = (uint8_t *)evt_buffer.data;
	rq->is_read = false;
	rq->offset = req->sector * _bdev.block_size();
	rq->opaque1 = evt;
	rq->opaque2 = this;
	
	if (!_bdev.submit_request(rq, handle_write_request_callback)) {
		ERROR << "Block device write request submission failed";
	}
}

void VirtIOBlockDevice::signal_completion()
{
	assert_interrupt(0);
	update_irq();
}

/*bool VirtIOBlockDevice::handle_write(uint64_t sector, uint8_t* buffer, uint32_t len)
{
	//DEBUG << CONTEXT(VirtIOBlockDevice) << "Handling Write: sector=" << std::hex << sector << ", len=" << len << ", buffer=" << std::hex << (uint64_t)buffer;
	AsyncBlockRequest *rq = new AsyncBlockRequest();
	rq->block_count = len / _bdev.block_size();
	rq->buffer = buffer;
	rq->is_read = false;
	rq->offset = sector * _bdev.block_size();
	rq->opaque = NULL;
	
	if (!_bdev.submit_request(rq, NULL)) return false;
	bool result = rq->request_complete.wait();
	delete rq;
	
	return result;
}
*/