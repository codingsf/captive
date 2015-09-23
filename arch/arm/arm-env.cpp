#include <arm-env.h>
#include <arm-cpu.h>
#include <devices/coco.h>

#include <printf.h>

using namespace captive;
using namespace captive::arch;
using namespace captive::arch::arm;

Environment *create_environment_arm(PerCPUData *per_cpu_data)
{
	return new arm_environment(arm_environment::ARMv7, per_cpu_data);
}

arm_environment::arm_environment(arm_variant variant, PerCPUData *per_cpu_data) : Environment(per_cpu_data), _variant(variant)
{
	// TODO: Abstract into platform-specific setup
	install_core_device(15, new devices::CoCo(*this));
}

arm_environment::~arm_environment()
{

}

CPU *arm_environment::create_cpu()
{
	return new arm_cpu(*this, per_cpu_data);
}
