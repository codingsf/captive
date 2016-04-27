/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   user-interface.h
 * Author: s0457958
 *
 * Created on 26 April 2016, 14:32
 */

#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <devices/net/network-interface.h>

namespace captive
{
	namespace devices
	{
		namespace net
		{
			namespace user
			{
				class UserInterface : public NetworkInterface
				{
				public:
					UserInterface();
					virtual ~UserInterface();
				
				private:
					bool transmit_packet(const uint8_t* buffer, uint32_t length) override;
				};
			}
		}
	}
}

#endif /* USER_INTERFACE_H */

