#include <env.h>
#include <cpu.h>
#include <printf.h>
#include <string.h>
#include <device.h>

using namespace captive::arch;

Environment::Environment()
{
	bzero(devices, sizeof(devices));
}

Environment::~Environment()
{

}

bool Environment::init()
{
	// IDT
	return true;
}

bool Environment::run(unsigned int ep)
{
	CPU *core = create_cpu();
	if (!core) {
		printf("error: unable to create core\n");
		return false;
	}

	if (!core->init(ep)) {
		printf("error: unable to init core\n");
		return false;
	}

	bool result = core->run();
	delete core;

	return result;
}

bool Environment::read_core_device(CPU& cpu, uint32_t id, uint32_t reg, uint32_t& data)
{
	if (id > 15 || devices[id] == NULL) {
		printf("attempted read of invalid device %d\n", id);
		return false;
	}

	return devices[id]->read(cpu, reg, data);
}

bool Environment::write_core_device(CPU& cpu, uint32_t id, uint32_t reg, uint32_t data)
{
	if (id > 15 || devices[id] == NULL) {
		printf("attempted write of invalid device %d\n", id);
		return false;
	}

	return devices[id]->write(cpu, reg, data);
}
