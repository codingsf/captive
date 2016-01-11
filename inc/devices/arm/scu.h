/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   scu.h
 * Author: s0457958
 *
 * Created on 07 January 2016, 11:43
 */
#ifndef SCU_H
#define SCU_H

#include <devices/device.h>

namespace captive {
	namespace devices {
		namespace arm {
			class SnoopControlUnit : public Device
			{
			public:
				SnoopControlUnit();
				virtual ~SnoopControlUnit();
				
				std::string name() const { return "scu"; }
				uint32_t size() const { return 0x100; }
				
				bool read(uint64_t off, uint8_t len, uint64_t& data);
				bool write(uint64_t off, uint8_t len, uint64_t data);
				
			private:
				uint32_t control;
			};
		}
	}
}

#endif /* SCU_H */
