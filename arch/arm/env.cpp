#include <arm-env.h>
#include <arm-cpu.h>
#include <devices/coco.h>

#include <printf.h>

using namespace captive::arch;
using namespace captive::arch::arm;

Environment *create_environment()
{
	return new ArmEnvironment();
}

ArmEnvironment::ArmEnvironment()
{
	// TODO: Abstract into platform-specific setup
	install_core_device(15, new devices::CoCo(*this));
}

ArmEnvironment::~ArmEnvironment()
{

}

CPU *ArmEnvironment::create_cpu()
{
	return new ArmCPU(*this);
}
