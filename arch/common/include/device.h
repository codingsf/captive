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
		class CPU;

		class Device
		{
		public:
			Device(Environment& env);
			virtual ~Device();

			inline Environment& env() const { return _env; }

		private:
			Environment& _env;
		};

		class CoreDevice : public Device
		{
		public:
			CoreDevice(Environment& env);
			virtual ~CoreDevice();

			virtual bool read(CPU& cpu, uint32_t reg, uint32_t& data) = 0;
			virtual bool write(CPU& cpu, uint32_t reg, uint32_t data) = 0;
		};
	}
}

#endif	/* DEVICE_H */

