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
			class AsyncBlockDevice;

			namespace virtio {
				class VirtIOBlockDevice : public VirtIO
				{
				public:
					VirtIOBlockDevice(irq::IRQLine& irq, AsyncBlockDevice& bdev);
					virtual ~VirtIOBlockDevice();
					
					void signal_completion();

				protected:
					void reset() override;
					const uint8_t* config_area() const override { return (const uint8_t *)&config; }
					uint32_t config_area_size() const override { return sizeof(config); }

					void process_event(VirtIOQueueEvent *evt) override;

				private:
					AsyncBlockDevice& _bdev;

					struct virtio_blk_req {
						uint32_t type;
						uint32_t ioprio;
						uint64_t sector;
					} __packed;

					void handle_read_request(VirtIOQueueEvent *evt, struct virtio_blk_req *req);
					void handle_write_request(VirtIOQueueEvent *evt, struct virtio_blk_req *req);
					
					struct {
						uint64_t capacity;
						uint32_t size_max;
						uint32_t seg_max;
						struct {
							uint16_t cylinders;
							uint8_t heads;
							uint8_t sectors;
						} geometry;
						uint32_t block_size;
					} __packed config;
				};
			}
		}
	}
}

#endif	/* VIRTIO_BLOCK_DEVICE_H */

