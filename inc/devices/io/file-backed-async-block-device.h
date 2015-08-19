/* 
 * File:   file-backed-async-block-device.h
 * Author: s0457958
 *
 * Created on 24 June 2015, 16:50
 */

#ifndef FILE_BACKED_ASYNC_BLOCK_DEVICE_H
#define	FILE_BACKED_ASYNC_BLOCK_DEVICE_H

#include <define.h>
#include <devices/io/async-block-device.h>

namespace captive {
	namespace devices {
		namespace io {
			class FileBackedAsyncBlockDevice : public AsyncBlockDevice
			{
			public:
				FileBackedAsyncBlockDevice();
				~FileBackedAsyncBlockDevice();
				
				bool submit_request(AsyncBlockRequest *rq, block_request_cb_t cb) override;

				uint64_t blocks() const override { return _block_count; }
				
				bool open_file(std::string filename, bool read_only = false);
				void close_file();

				bool read_only() const override { return _read_only; }

			private:
				int _file_descr;
				uint64_t _file_size;
				uint64_t _block_count;
				bool _read_only;
			};
		}
	}
}

#endif	/* FILE_BACKED_ASYNC_BLOCK_DEVICE_H */
