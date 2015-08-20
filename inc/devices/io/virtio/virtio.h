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
#include <atomic>
#include <util/completion.h>

//#define SYNCHRONOUS

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

				class VirtQueue;
				class VirtIOQueueEvent
				{
				public:
					VirtIOQueueEvent(VirtQueue *queue, uint32_t descr_idx) : response_size(0), queue(queue), descr_idx(descr_idx) { }

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
					VirtQueue *queue;
					uint32_t descr_idx;
					
					util::Completion<bool> complete;
					
					void submit();
				};

				class VirtIO : public Device
				{
					friend class VirtIOQueueEvent;
					friend class VirtQueue;
					
				public:
					VirtIO(irq::IRQLine& irq, uint32_t version, uint32_t device_id, uint8_t nr_queues);
					virtual ~VirtIO();

					bool read(uint64_t off, uint8_t len, uint64_t& data) override;
					bool write(uint64_t off, uint8_t len, uint64_t data) override;

					virtual uint32_t size() const { return 0x1000; }

				protected:
					virtual void reset() = 0;

					virtual const uint8_t *config_area() const = 0;
					virtual uint32_t config_area_size() const = 0;

					inline VirtQueue *current_queue() const { return queue(_queue_sel); }
					inline VirtQueue *queue(uint8_t index) const { if (index > queues.size()) return NULL; else return queues[index]; }

					virtual void process_event(VirtIOQueueEvent *evt) = 0;
					virtual void submit_event(VirtIOQueueEvent *evt);

					inline void assert_interrupt(int idx) {
						_isr |= 1 << idx;
					}

					inline void rescind_interrupt(int idx) {
						_isr &= ~(1 << idx);
					}

					inline void set_host_feature(int idx) {
						_host_features |= 1 << idx;
					}

					inline void clear_host_feature(int idx) {
						_host_features &= ~(1 << idx);
					}

				private:
					void process_queue(VirtQueue *queue);
					void update_irq();

					std::vector<VirtQueue *> queues;
					irq::IRQLine& _irq;

					std::atomic<uint32_t> _isr;

					uint32_t _host_features, _guest_page_shift;

					uint32_t _version;
					uint32_t _device_id;
					uint32_t _vendor_id;

					uint32_t _guest_features;
					uint32_t _guest_features_sel;

					uint32_t _queue_sel;

					uint32_t _status;
				};
			}
		}
	}
}

#endif	/* VIRTIO_H */

