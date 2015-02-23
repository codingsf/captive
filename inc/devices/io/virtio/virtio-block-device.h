/*
 * File:   virtio-block-device.h
 * Author: spink
 *
 * Created on 23 February 2015, 15:52
 */

#ifndef VIRTIO_BLOCK_DEVICE_H
#define	VIRTIO_BLOCK_DEVICE_H

#include <devices/io/virtio/virtio.h>

namespace captive {
	namespace devices {
		namespace io {
			class BlockDevice;

			namespace virtio {
				class VirtIOBlockDevice : public VirtIO
				{
				public:
					VirtIOBlockDevice(irq::IRQLine& irq, BlockDevice& bdev);
					virtual ~VirtIOBlockDevice();

				protected:
					void reset() override;
					uint8_t* config_area() const override;
					uint32_t config_area_size() const override;

				private:
					void process_event(VirtIOQueueEvent& evt) override;
				};
			}
		}
	}
}

#endif	/* VIRTIO_BLOCK_DEVICE_H */

