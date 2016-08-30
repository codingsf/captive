#include <devices/io/virtio/virtio-net-device.h>
#include <devices/io/virtio/virtqueue.h>
#include <captive.h>

USE_CONTEXT(VirtIO);
DECLARE_CHILD_CONTEXT(VirtIONetworkDevice, VirtIO);

using namespace captive::devices::io::virtio;

VirtIONetworkDevice::VirtIONetworkDevice(irq::IRQLine& irq, net::NetworkInterface& iface) 
	: VirtIO(irq, 1, 1, 2),
	_iface(iface)
{
	bzero(&config, sizeof(config));
	
	config.mac[0] = 0x80;
	config.mac[1] = 0x81;
	config.mac[2] = 0x82;
	config.mac[3] = 0x83;
	config.mac[4] = 0x84;
	config.mac[5] = 0x85;
	
	config.status = 1;

	set_host_feature(5);	// MAC
	set_host_feature(16);	// STATUS
	
	iface.attach(*this);
}

VirtIONetworkDevice::~VirtIONetworkDevice()
{

}

void VirtIONetworkDevice::reset()
{

}

void VirtIONetworkDevice::process_event(VirtIOQueueEvent* evt)
{
	if (evt->queue->index() == 0) {
		std::unique_lock<std::mutex> l(_receive_buffer_lock);
		_receive_buffers.push_back(evt);
	} else if (evt->queue->index() == 1) {
		VirtIOQueueEventBuffer& buffer = evt->read_buffers.back();
		_iface.transmit_packet((const uint8_t *)buffer.data, buffer.size);
		submit_event(evt);
	}
}

void VirtIONetworkDevice::receive_packet(const uint8_t* buffer, uint32_t length)
{
	_receive_buffer_lock.lock();
	
	if (_receive_buffers.size() == 0) {
		_receive_buffer_lock.unlock();
		return;
	}
	
	VirtIOQueueEvent *evt = _receive_buffers.front();
	_receive_buffers.pop_front();
	
	_receive_buffer_lock.unlock();
	
	if (evt->write_buffers.size() != 2) {
		fprintf(stderr, "****xxxxx****\n");
		return;
	}
	
	fprintf(stderr, "buffer length:%d\n", length);
	
	VirtIOQueueEventBuffer& hdr_buffer = evt->write_buffers.front();
	fprintf(stderr, "net_hdr size:%d\n", hdr_buffer.size);

	virtio_net_hdr *net_hdr = (virtio_net_hdr *)hdr_buffer.data;	
	net_hdr->num_buffers = 2;
	
	VirtIOQueueEventBuffer& io_buffer = evt->write_buffers.back();
	fprintf(stderr, "io_buffer size:%d\n", io_buffer.size);
	
	unsigned int used_length = (length > io_buffer.size) ? io_buffer.size : length;
	memcpy(io_buffer.data, buffer, used_length);
	
	evt->response_size = hdr_buffer.size + used_length;
	submit_event(evt);
}
