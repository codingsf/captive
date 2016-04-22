/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   network-device.h
 * Author: s0457958
 *
 * Created on 22 April 2016, 15:04
 */

#ifndef NETWORK_DEVICE_H
#define NETWORK_DEVICE_H

#include <devices/device.h>

namespace captive
{
	namespace devices
	{
		namespace net
		{
			class NetworkInterface;
			
			class NetworkDevice : public Device
			{
				friend class NetworkInterface;
				
			private:
				virtual bool receive_packet(const uint8_t *buffer, uint32_t length) = 0;
			};
		}
	}
}

#endif /* NETWORK_DEVICE_H */

