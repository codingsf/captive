#include <arm-env.h>
#include <arm-cpu.h>
#include <printf.h>

using namespace captive::arch;
using namespace captive::arch::arm;

Environment *create_environment()
{
	printf("create environment\n");
	return new ArmEnvironment();
}

ArmEnvironment::ArmEnvironment()
{

}

ArmEnvironment::~ArmEnvironment()
{

}

CPU *ArmEnvironment::create_cpu()
{
	printf("create cpu\n");
	return new ArmCPU(*this);
}
