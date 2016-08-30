/* 
 * File:   virtio-net-device.h
 * Author: spink
 *
 * Created on 28 August 2016, 09:10
 */

#ifndef VIRTIO_NET_DEVICE_H
#define VIRTIO_NET_DEVICE_H

#include <devices/io/virtio/virtio.h>
#include <devices/net/network-interface.h>

#include <list>
#include <mutex>

namespace captive {
	namespace devices {
		namespace io {		
			namespace virtio {
				class VirtIONetworkDevice : public VirtIO, public net::NetworkInterfaceReceiveCallback
				{
				public:
					VirtIONetworkDevice(irq::IRQLine& irq, net::NetworkInterface& iface);
					virtual ~VirtIONetworkDevice();
					
					void receive_packet(const uint8_t *buffer, uint32_t length) override;
					
				protected:
					void reset() override;
					const uint8_t* config_area() const override { return (const uint8_t *)&config; }
					uint32_t config_area_size() const override { return sizeof(config); }

					void process_event(VirtIOQueueEvent *evt) override;

				private:
					net::NetworkInterface& _iface;
					
					std::mutex _receive_buffer_lock;
					std::list<VirtIOQueueEvent *> _receive_buffers;
					
					struct {
						uint8_t mac[6];
						uint16_t status;
					} __packed config;
				};
			}
		}
	}
}

#endif /* VIRTIO_NET_DEVICE_H */

