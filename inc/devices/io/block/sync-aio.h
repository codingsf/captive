/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   sync-aio.h
 * Author: s0457958
 *
 * Created on 21 March 2016, 15:20
 */

#ifndef SYNC_AIO_H
#define SYNC_AIO_H

#include <devices/io/block/aio.h>

namespace captive {
	namespace devices {
		namespace io {
			namespace block {
				class SyncAIO : public AIO
				{
				public:
					SyncAIO(int fd, BlockDevice& bdev);
					~SyncAIO();
					
					bool init() override;
					bool submit(BlockDeviceRequest *rq, BlockDevice::block_request_cb_t callback) override;
				};
			}
		}
	}
}

#endif /* SYNC_AIO_H */

