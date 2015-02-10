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
	printf("cpu run\n");

	do {
		uint32_t pc = read_pc();

		printf("exec: %x\n", pc);

	} while(false);

	return true;
}

bool ArmCPU::init(unsigned int ep)
{
	printf("cpu init @ %x\n", ep);
	_ep = ep;

	regs.RB[15] = ep;

	return true;
}
