/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   network-interface.h
 * Author: s0457958
 *
 * Created on 19 April 2016, 16:42
 */

#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

#include <define.h>

namespace captive {
	namespace devices {
		namespace net {
			class NetworkDevice;

			class NetworkInterface {
			public:

				NetworkInterface() : attached_device(NULL) {
				}

				virtual ~NetworkInterface() {
				}

				void attach(NetworkDevice *device);

				virtual bool transmit_packet(const uint8_t *buffer, uint32_t length) = 0;

				virtual bool start() = 0;
				virtual void stop() = 0;

			protected:

				NetworkDevice *device() const {
					return attached_device;
				}

			private:
				NetworkDevice *attached_device;
			};
		}
	}
}

#endif /* NETWORK_INTERFACE_H */

