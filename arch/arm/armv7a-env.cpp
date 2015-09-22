#include <armv7a-env.h>
#include <armv7a-cpu.h>
#include <devices/coco.h>

#include <printf.h>

using namespace captive;
using namespace captive::arch;
using namespace captive::arch::armv7a;

Environment *create_environment_armv7a(PerCPUData *per_cpu_data)
{
	return new armv7a_environment(per_cpu_data);
}

armv7a_environment::armv7a_environment(PerCPUData *per_cpu_data) : Environment(per_cpu_data)
{
	// TODO: Abstract into platform-specific setup
	install_core_device(15, new armv5e::devices::CoCo(*this));
}

armv7a_environment::~armv7a_environment()
{

}

CPU *armv7a_environment::create_cpu()
{
	return new armv7a_cpu(*this, per_cpu_data);
}
