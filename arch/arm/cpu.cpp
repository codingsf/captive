#include <arm-cpu.h>
#include <arm-env.h>
#include <printf.h>

using namespace captive::arch::arm;

ArmCPU::ArmCPU(ArmEnvironment& env) : CPU(env)
{

}

ArmCPU::~ArmCPU()
{

}

bool ArmCPU::run()
{
	printf("CPU RUN\n");
	return false;
}

bool ArmCPU::init(unsigned int ep)
{
	printf("cpu init @ %x\n", ep);
	_ep = ep;
	
	return true;
}
