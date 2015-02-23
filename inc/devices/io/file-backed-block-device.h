/*
 * File:   file-backed-block-device.h
 * Author: spink
 *
 * Created on 23 February 2015, 15:23
 */

#ifndef FILE_BACKED_BLOCK_DEVICE_H
#define	FILE_BACKED_BLOCK_DEVICE_H

#include <define.h>
#include <devices/io/block-device.h>

namespace captive {
	namespace devices {
		namespace io {
			class FileBackedBlockDevice : public BlockDevice
			{
			public:
				FileBackedBlockDevice();
				~FileBackedBlockDevice();

				uint64_t blocks() const override { return _block_count; }

				bool read_block(uint64_t block_idx, uint8_t* buffer) override;
				bool read_blocks(uint64_t block_idx, uint32_t count, uint8_t* buffer) override;
				bool write_block(uint64_t block_idx, const uint8_t* buffer) override;
				bool write_blocks(uint64_t block_idx, uint32_t count, const uint8_t* buffer) override;

				bool open_file(std::string filename, bool read_only = false);
				void close_file();

				bool read_only() const override { return _read_only; }

			private:
				inline void *get_data_ptr(uint64_t block_idx) const {
					return (void *)((uint64_t)_file_data + calculate_byte_offset(block_idx));
				}

				int _file_descr;
				void *_file_data;
				uint64_t _file_size;
				uint64_t _block_count;
				bool _use_mmap;
				bool _read_only;
			};
		}
	}
}

#endif	/* FILE_BACKED_BLOCK_DEVICE_H */

