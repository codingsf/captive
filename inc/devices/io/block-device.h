/*
 * File:   block-device.h
 * Author: spink
 *
 * Created on 23 February 2015, 15:20
 */

#ifndef BLOCK_DEVICE_H
#define	BLOCK_DEVICE_H

#include <define.h>

namespace captive {
	namespace devices {
		namespace io {
			class BlockDevice
			{
			public:
				BlockDevice(uint32_t block_size = 4096);
				virtual ~BlockDevice();

				virtual bool read_block(uint64_t block_idx, uint8_t *buffer) = 0;
				virtual bool read_blocks(uint64_t block_idx, uint32_t count, uint8_t *buffer) = 0;
				virtual bool write_block(uint64_t block_idx, const uint8_t *buffer) = 0;
				virtual bool write_blocks(uint64_t block_idx, uint32_t count, const uint8_t *buffer) = 0;

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

#endif	/* BLOCK_DEVICE_H */

