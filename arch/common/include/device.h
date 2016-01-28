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
		
		extern void mmio_device_read(gpa_t pa, uint8_t size, uint64_t& value);
		extern void mmio_device_write(gpa_t pa, uint8_t size, uint64_t value);

		extern int handle_fast_device_read(struct mcontext *mctx);
		extern int handle_fast_device_write(struct mcontext *mctx);
	}
}

#endif	/* DEVICE_H */

