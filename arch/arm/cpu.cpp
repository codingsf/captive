//#define TRACE

#include <arm-cpu.h>
#include <arm-env.h>
#include <arm-decode.h>
#include <arm-interp.h>
#include <arm-disasm.h>
#include <new>

#include <printf.h>
#include <string.h>

using namespace captive::arch::arm;

ArmCPU::ArmCPU(ArmEnvironment& env) : CPU(env)
{

}

ArmCPU::~ArmCPU()
{

}

void ArmCPU::dump_state() const
{
	printf("*** CPU STATE ***\n");

	for (int i = 0; i < 16; i++) {
		printf("  r%d = %x (%d)\n", i, state.regs.RB[i], state.regs.RB[i]);
	}
}

bool ArmCPU::run()
{
	bool step_ok = true;

	char insn_data[512];
	bzero(insn_data, sizeof(insn_data));

	ArmDecode *insn = (ArmDecode *)insn_data;
	ArmInterp interp(*this);
	ArmDisasm disasm;

	printf("constructing default decode\n");
	new (insn) ArmDecode();

	printf("cpu run\n");

	do {
		state.last_exception_action = 0;

		insn->decode(ArmDecode::arm, state.regs.RB[15]);

#ifdef TRACE
		printf("[%08x] %08x %30s ", state.regs.RB[15], insn->ir, disasm.disassemble(state.regs.RB[15], *insn));
#endif
		step_ok = interp.step_single(*insn);

#ifdef TRACE
		printf("\n");
#endif
	} while(step_ok);

	return true;
}

bool ArmCPU::init(unsigned int ep)
{
	printf("cpu init @ %x\n", ep);
	_ep = ep;

	volatile uint32_t *mem = (volatile uint32_t *)0;
	*mem++ = 0xef000000;
	*mem++ = 0xe1a00000;
	*mem++ = 0xe12fff1c;

	printf("clearing state\n");
	bzero(&state, sizeof(state));

	state.regs.RB[1] = 0x25e;
	state.regs.RB[2] = 0x1234;
	state.regs.RB[12] = ep;
	state.regs.RB[15] = 0;

	return true;
}

void ArmCPU::handle_angel_syscall()
{
	printf("ANGEL\n");
}
