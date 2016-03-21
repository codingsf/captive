/*
 * File:   virtqueue.h
 * Author: spink
 *
 * Created on 23 February 2015, 16:29
 */

#ifndef VIRTQUEUE_H
#define	VIRTQUEUE_H

#include <define.h>
#include <mutex>

namespace captive
{
	namespace devices
	{
		namespace io
		{
			namespace virtio
			{
				struct VirtRingDescr
				{
					uint64_t addr;
					uint32_t length;
					
#define VRING_DESC_F_NEXT		1
#define VRING_DESC_F_WRITE		2
#define VRING_DESC_F_INDIRECT	4
					
					uint16_t flags;
					uint16_t next;
					
					inline bool has_next() const { return !!(flags & VRING_DESC_F_NEXT); }
					inline bool is_write() const { return !!(flags & VRING_DESC_F_WRITE); }
					inline bool is_indirect() const { return !!(flags & VRING_DESC_F_INDIRECT); }
				} __packed;
				
				struct VirtRingAvail
				{
#define VRING_AVAIL_F_NO_INTERRUPT	1
					
					uint16_t flags;
					uint16_t index;
					uint16_t ring[];
				} __packed;
				
				struct VirtRingUsedElem
				{
					uint32_t id;
					uint32_t len;
				} __packed;
				
				struct VirtRingUsed
				{
					uint16_t flags;
					uint16_t idx;
					VirtRingUsedElem ring[];
				} __packed;
				
				class VirtIO;
				class VirtQueue
				{
				public:
					VirtQueue(VirtIO& owner) : _owner(owner), _queue_num(0), _queue_align(0), _guest_phys_addr(0), _queue_host_addr(NULL) { }
					
					inline uint32_t guest_phys_addr() const { return _guest_phys_addr; }
					inline void guest_phys_addr(uint32_t gpa) { _guest_phys_addr = gpa; }
					
					inline uint32_t num() const { return _queue_num; }
					inline void num(uint32_t num) { _queue_num = num; }
					
					inline uint32_t align() const { return _queue_align; }
					inline void align(uint32_t align) { _queue_align = align; }
					
					inline void *queue_host_addr() const { return _queue_host_addr; }
					inline void queue_host_addr(void *addr) { _queue_host_addr = addr; init_vring(); }
					
					inline VirtRingDescr *pop(uint32_t& idx)
					{
						std::unique_lock<std::mutex> lock(_pop_mtx);
						
						__barrier();

						uint16_t num_heads = _avail_descrs->index - prev_idx;
						assert(num_heads < _queue_num);
						
						if (num_heads == 0) {
							return NULL;
						}
						
						uint16_t head = _avail_descrs->ring[prev_idx % _queue_num];
						assert(head < _queue_num);
						
						idx = head;
						
						prev_idx++;
						return get_descr(head);
					}
					
					inline void push(uint32_t elem_idx, uint32_t size)
					{
						std::unique_lock<std::mutex> lock(_push_mtx);
						
						assert(elem_idx < _queue_num);
						
						__barrier();

						uint16_t idx = _used_descrs->idx % _queue_num;
						_used_descrs->ring[idx].id = elem_idx;
						_used_descrs->ring[idx].len = size;
						
						_used_descrs->idx++;
					}
					
					inline VirtRingDescr *get_descr(uint32_t idx) { 
						assert(idx < _queue_num);
						
						if (!_queue_host_addr) return NULL;
						
						return &_vring_descrs[idx];
					};

					inline VirtIO& owner() const { return _owner; }
					
				private:
					VirtIO& _owner;
					uint32_t _queue_num;
					uint32_t _queue_align;
					uint32_t _guest_phys_addr;
					void *_queue_host_addr;
					
					VirtRingDescr *_vring_descrs;
					VirtRingAvail *_avail_descrs;
					VirtRingUsed *_used_descrs;
					uint16_t prev_idx;
					
					std::mutex _mtx, _push_mtx, _pop_mtx;
					
					inline void init_vring()
					{
						_vring_descrs = (VirtRingDescr *)_queue_host_addr;
						_avail_descrs = (VirtRingAvail *)((uint64_t)_vring_descrs + (sizeof(VirtRingDescr) * _queue_num));
						
						uint64_t avail_size = (_queue_num * sizeof(uint16_t)) + 4;
						uint64_t padding = 4096 - (avail_size % 4096);
						
						_used_descrs = (VirtRingUsed *)((uint64_t)_avail_descrs + avail_size + padding);
						
						prev_idx = 0;
					}
				};
			}
		}
	}
}

#endif	/* VIRTQUEUE_H */
