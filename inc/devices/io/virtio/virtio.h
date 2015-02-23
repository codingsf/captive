/*
 * File:   virtio.h
 * Author: spink
 *
 * Created on 23 February 2015, 15:27
 */

#ifndef VIRTIO_H
#define	VIRTIO_H

#include <devices/device.h>
#include <vector>

namespace captive {
	namespace devices {
		namespace irq {
			class IRQLine;
		}

		namespace io {
			namespace virtio {
				struct VirtIOQueueEventBuffer
				{
				public:
					VirtIOQueueEventBuffer(void *data, uint32_t size) : data(data), size(size) { }

					void *data;
					uint32_t size;
				};

				class VirtIOQueueEvent
				{
				public:
					VirtIOQueueEvent() : response_size(0) { }

					~VirtIOQueueEvent() {
					}

					inline void add_read_buffer(void *data, uint32_t size) {
						read_buffers.push_back(VirtIOQueueEventBuffer(data, size));
					}

					inline void add_write_buffer(void *data, uint32_t size) {
						write_buffers.push_back(VirtIOQueueEventBuffer(data, size));
					}

					inline void clear() {
						response_size = 0;
						read_buffers.clear();
						write_buffers.clear();
					}

					std::vector<VirtIOQueueEventBuffer> read_buffers;
					std::vector<VirtIOQueueEventBuffer> write_buffers;

					uint32_t response_size;
				};

				class VirtQueue;
				class VirtIO : public Device
				{
				public:
					VirtIO(irq::IRQLine& irq, uint32_t version, uint32_t device_id, uint8_t nr_queues);
					virtual ~VirtIO();

					bool read(uint64_t off, uint8_t len, uint64_t& data) override;
					bool write(uint64_t off, uint8_t len, uint64_t data) override;

					virtual uint32_t size() const { return 0x1000; }

				protected:
					virtual void reset() = 0;

					virtual uint8_t *config_area() const = 0;
					virtual uint32_t config_area_size() const = 0;

					inline VirtQueue *current_queue() const { return queue(QueueSel); }
					inline VirtQueue *queue(uint8_t index) const { if (index > queues.size()) return NULL; else return queues[index]; }

					inline void assert_interrupt(uint32_t i) {
						InterruptStatus |= i;
					}

					uint32_t host_features;

				private:
					void process_queue(VirtQueue *queue);
					virtual void process_event(VirtIOQueueEvent& evt) = 0;

					uint8_t guest_page_shift;

					std::vector<VirtQueue *> queues;

					irq::IRQLine& _irq;

					uint32_t Version;
					uint32_t DeviceID;
					uint32_t VendorID;
					
					uint32_t HostFeaturesSel;

					uint32_t GuestFeatures;
					uint32_t GuestFeaturesSel;

					uint32_t QueueSel;

					uint32_t InterruptStatus;
					uint32_t InterruptACK;
					uint32_t Status;
				};
			}
		}
	}
}

#endif	/* VIRTIO_H */

