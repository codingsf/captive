#include <arm-cpu.h>
#include <arm-env.h>
#include <arm-decode.h>
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

		cur_insn.decode(pc);
		switch (cur_insn.opcode) {
		case ArmDecode::UNKNOWN:
		default:
			printf("cpu unknown instruction: %x\n", cur_insn.ir);
			return false;
		}

	} while(true);

	return true;
}

bool ArmCPU::init(unsigned int ep)
{
	printf("cpu init @ %x\n", ep);
	_ep = ep;

	regs.RB[15] = ep;

	return true;
}
