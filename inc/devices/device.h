/*
 * File:   device.h
 * Author: spink
 *
 * Created on 12 February 2015, 19:58
 */

#ifndef DEVICE_H
#define	DEVICE_H

#include <define.h>
#include <string>

namespace captive {
	namespace hypervisor {
		class Guest;
	}

	namespace devices {
		class Device
		{
		public:
			Device();
			virtual ~Device();

			inline void attach(hypervisor::Guest& guest) {
				_guest = &guest;
			}

			inline hypervisor::Guest& guest() const {
				return *_guest;
			}

			virtual bool read(uint64_t off, uint8_t len, uint64_t& data) = 0;
			virtual bool write(uint64_t off, uint8_t len, uint64_t data) = 0;

			virtual std::string name() const { return "(unknown)"; }

		private:
			hypervisor::Guest *_guest;
		};
	}
}

#endif	/* DEVICE_H */

