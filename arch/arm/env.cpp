#include <arm-env.h>
#include <arm-cpu.h>
#include <devices/coco.h>
#include <profile/image.h>

#include <printf.h>

using namespace captive;
using namespace captive::arch;
using namespace captive::arch::arm;

Environment *create_environment(PerCPUData *per_cpu_data)
{
	return new ArmEnvironment(per_cpu_data);
}

ArmEnvironment::ArmEnvironment(PerCPUData *per_cpu_data) : Environment(per_cpu_data), _profile_image(new profile::Image())
{
	// TODO: Abstract into platform-specific setup
	install_core_device(15, new devices::CoCo(*this));
}

ArmEnvironment::~ArmEnvironment()
{

}

CPU *ArmEnvironment::create_cpu()
{
	return new ArmCPU(*this, *_profile_image, per_cpu_data);
}
