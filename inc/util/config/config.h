/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   config.h
 * Author: s0457958
 *
 * Created on 01 September 2016, 09:58
 */

#ifndef VCONFIG_H
#define VCONFIG_H

#include <util/maybe.h>

namespace captive
{
	namespace util
	{
		namespace config
		{
			class Configuration
			{
			public:
				maybe<std::string> arch_module;
				maybe<std::string> guest_kernel;
				
				maybe<std::string> block_device_file;
				
				maybe<std::string> net_mac_addr;
				maybe<std::string> net_tap_device;
			};
		}
	}
}

#endif /* CONFIG_H */

