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
		fprintf(stderr, "*** TRANSMIT rd=%d, wr=%d\n", evt->read_buffers.size(), evt->write_buffers.size());
		
		fprintf(stderr, "[0] ");
		VirtIOQueueEventBuffer buffer = evt->read_buffers.front();
		for (unsigned i = 0; i < buffer.size; i++) {
			fprintf(stderr, "%02x ", ((uint8_t *)buffer.data)[i]);
		}
		fprintf(stderr, "\n");

		fprintf(stderr, "[1] ");
		buffer = evt->read_buffers.back();
		for (unsigned i = 0; i < buffer.size; i++) {
			fprintf(stderr, "%02x ", ((uint8_t *)buffer.data)[i]);
		}
		fprintf(stderr, "\n");
	}
}
