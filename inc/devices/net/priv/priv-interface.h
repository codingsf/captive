/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   priv-interface.h
 * Author: s0457958
 *
 * Created on 07 July 2016, 11:41
 */

#ifndef PRIV_INTERFACE_H
#define PRIV_INTERFACE_H

#include <devices/net/network-interface.h>
#include <string>

namespace captive
{
	namespace devices
	{
		namespace net
		{
			namespace priv
			{
				class PrivateInterface : public NetworkInterface
				{
				public:
					PrivateInterface();
					virtual ~PrivateInterface();
					
					bool open(std::string socket);
				
				private:
					bool transmit_packet(const uint8_t* buffer, uint32_t length) override;
				};
			}
		}
	}
}

#endif /* PRIV_INTERFACE_H */

