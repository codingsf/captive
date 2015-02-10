#include <env.h>
#include <cpu.h>
#include <printf.h>

using namespace captive::arch;

Environment::Environment()
{

}

Environment::~Environment()
{

}

bool Environment::run(unsigned int ep)
{
	printf("environment run\n");

	CPU *core = create_cpu();
	if (!core->init(ep)) {
		printf("unable to init core\n");
		return false;
	}

	core->run();

	return true;
}
