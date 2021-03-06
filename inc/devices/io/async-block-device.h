/* 
 * File:   async-block-device.h
 * Author: s0457958
 *
 * Created on 24 June 2015, 16:47
 */

#ifndef ASYNC_BLOCK_DEVICE_H
#define	ASYNC_BLOCK_DEVICE_H

#include <define.h>
#include <util/completion.h>

namespace captive {
	namespace devices {
		namespace io {
			struct AsyncBlockRequest
			{
				uint64_t block_offset;
				uint32_t block_count;
				uint8_t *buffer;
				bool is_read;
				void *opaque;
			};
			
			class AsyncBlockDevice
			{
			public:
				typedef void (*block_request_cb_t)(AsyncBlockRequest *rq, bool success);
				
				AsyncBlockDevice(uint32_t block_size = 4096);
				virtual ~AsyncBlockDevice();
				
				virtual bool submit_request(AsyncBlockRequest *rq, block_request_cb_t cb) = 0;

				inline uint32_t block_size() const { return _block_size; }

				virtual uint64_t blocks() const = 0;
				virtual bool read_only() const = 0;

			protected:
				inline uint64_t calculate_byte_offset(uint64_t block_idx) const __attribute__((pure)) {
					return _block_size * block_idx;
				}

			private:
				uint32_t _block_size;				
			};
		}
	}
}

#endif	/* ASYNC_BLOCK_DEVICE_H */

