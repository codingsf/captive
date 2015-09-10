#include <armv5e-env.h>
#include <armv5e-cpu.h>
#include <devices/coco.h>

#include <printf.h>

using namespace captive;
using namespace captive::arch;
using namespace captive::arch::armv5e;

Environment *create_environment(PerCPUData *per_cpu_data)
{
	return new armv5e_environment(per_cpu_data);
}

armv5e_environment::armv5e_environment(PerCPUData *per_cpu_data) : Environment(per_cpu_data)
{
	// TODO: Abstract into platform-specific setup
	install_core_device(15, new devices::CoCo(*this));
}

armv5e_environment::~armv5e_environment()
{

}

CPU *armv5e_environment::create_cpu()
{
	return new armv5e_cpu(*this, per_cpu_data);
}
