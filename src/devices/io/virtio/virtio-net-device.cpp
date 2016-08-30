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
		VirtIOQueueEventBuffer buffer = evt->read_buffers.back();
		_iface.transmit_packet((const uint8_t *)buffer.data, buffer.size);
		evt->submit();
	}
}

void VirtIONetworkDevice::receive_packet(const uint8_t* buffer, uint32_t length)
{
	fprintf(stderr, "************ virtio receive packet\n");
}
