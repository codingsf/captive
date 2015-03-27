/*
 * File:   virtqueue.h
 * Author: spink
 *
 * Created on 23 February 2015, 16:29
 */

#ifndef VIRTQUEUE_H
#define	VIRTQUEUE_H

#include <define.h>

namespace captive
{
	namespace devices
	{
		namespace io
		{
			namespace virtio
			{
				class VirtQueue;
				class VirtRing
				{
				public:
					struct VirtRingDesc {
						uint64_t paddr;
						uint32_t len;
						uint16_t flags;
						uint16_t next;
					} __packed;

					struct VirtRingAvail {
						uint16_t flags;
						uint16_t idx;
						uint16_t ring[];
					} __packed;

					struct VirtRingUsedElem {
						uint32_t id;
						uint32_t len;
					} __packed;

					struct VirtRingUsed {
						uint16_t flags;
						uint16_t idx;
						VirtRingUsedElem ring[];
					} __packed;

					inline struct VirtRingDesc *GetDescriptor(uint8_t index) const {
						return &descriptors[index];
					}

					inline struct VirtRingAvail *GetAvailable() const {
						return avail;
					}

					inline struct VirtRingUsed *GetUsed() const {
						return used;
					}

					inline void SetHostBaseAddress(void *base_address, uint32_t size) {
						assert(size);

						descriptors = (VirtRingDesc *)base_address;
						avail = (VirtRingAvail *)((unsigned long)base_address + size * sizeof(VirtRingDesc));

						uint32_t avail_size = (size * sizeof(uint16_t)) + 4;
						uint32_t padding = 4096 - (avail_size % 4096);

						used = (VirtRingUsed *)((unsigned long)avail + avail_size + padding);

					}

				private:
					VirtRingDesc *descriptors;
					VirtRingAvail *avail;
					VirtRingUsed *used;
				};

				class VirtQueue
				{
				public:
					VirtQueue() : guest_base_address(0), host_base_address(NULL), align(0), size(0), last_avail_idx(0) { }

					inline void SetBaseAddress(gpa_t guest_base_address, void *host_base_address) {
						assert(size);

						this->guest_base_address = guest_base_address;
						this->host_base_address = host_base_address;

						ring.SetHostBaseAddress(host_base_address, size);
					}

					inline gpa_t GetPhysAddr() const { return guest_base_address; }

					inline void SetAlign(uint32_t align) { this->align = align; }

					inline void SetSize(uint32_t size) { this->size = size; }

					inline const VirtRing::VirtRingDesc *PopDescriptorChainHead(uint16_t& out_idx) {
						uint16_t num_heads = ring.GetAvailable()->idx - last_avail_idx;
						assert(num_heads < size);

						if (num_heads == 0)
							return NULL;

						uint16_t head = ring.GetAvailable()->ring[last_avail_idx % size];
						assert(head < size);

						out_idx = head;

						last_avail_idx++;
						return ring.GetDescriptor(head);
					}

					inline const VirtRing::VirtRingDesc *GetDescriptor(uint8_t index) {
						return ring.GetDescriptor(index);
					}

					inline void Push(uint16_t elem_idx, uint32_t len) {
						uint16_t idx = ring.GetUsed()->idx % size;

						ring.GetUsed()->ring[idx].id = elem_idx;
						ring.GetUsed()->ring[idx].len = len;

						uint16_t old = ring.GetUsed()->idx;
						ring.GetUsed()->idx = old + 1;
					}

				private:
					VirtRing ring;

					gpa_t guest_base_address;
					void *host_base_address;

					uint32_t align;
					uint32_t size;

					uint16_t last_avail_idx;
				};
			}
		}
	}
}

#endif	/* VIRTQUEUE_H */
