/*
 * File:   device.h
 * Author: spink
 *
 * Created on 12 February 2015, 10:49
 */

#ifndef DEVICE_H
#define	DEVICE_H

#include <define.h>

namespace captive
{
	namespace arch
	{
		class Environment;

		class Device
		{
		public:
			Device(Environment& env);
			virtual ~Device();

			virtual bool read(uint32_t reg, uint32_t& data) = 0;
			virtual bool write(uint32_t reg, uint32_t data) = 0;

			inline Environment& env() const { return _env; }

		private:
			Environment& _env;
		};
	}
}

#endif	/* DEVICE_H */

