/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   posix-aio.h
 * Author: s0457958
 *
 * Created on 22 February 2016, 16:24
 */

#ifndef POSIX_AIO_H
#define POSIX_AIO_H

#include <devices/io/block/aio.h>

#include <thread>
#include <aio.h>

namespace captive {
	namespace devices {
		namespace io {
			namespace block {
				class POSIXAIO : public AIO
				{
				public:
					POSIXAIO(int fd, BlockDevice& bdev);
					~POSIXAIO();
					
					bool init() override;
					bool submit(BlockDeviceRequest *rq, BlockDevice::block_request_cb_t callback) override;
					
				private:
					static void posix_aio_notify(sigval_t sv);
					
					struct BlockDeviceRequestContext
					{
						BlockDeviceRequestContext(BlockDeviceRequest *rq, BlockDevice::block_request_cb_t cb) : rq(rq), cb(cb) { }

						BlockDeviceRequest *rq;
						BlockDevice::block_request_cb_t cb;
						
						struct aiocb aio_cb;
					};
				};
			}
		}
	}
}

#endif /* POSIX_AIO_H */

