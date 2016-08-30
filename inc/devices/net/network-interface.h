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
			
			class NetworkInterfaceReceiveCallback
			{
			public:
				virtual void receive_packet(const uint8_t *buffer, uint32_t length) = 0;
			};

			class NetworkInterface {
			public:

				NetworkInterface() : _receive_callback(NULL) {
				}

				virtual ~NetworkInterface() {
				}

				void attach(NetworkInterfaceReceiveCallback& callback) {
					_receive_callback = &callback;
				}

				virtual bool transmit_packet(const uint8_t *buffer, uint32_t length) = 0;

				virtual bool start() = 0;
				virtual void stop() = 0;

			protected:
				void invoke_receive(const uint8_t *buffer, uint32_t length);
				
			private:
				NetworkInterfaceReceiveCallback *_receive_callback;
			};
		}
	}
}

#endif /* NETWORK_INTERFACE_H */

