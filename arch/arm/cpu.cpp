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
	ArmInterp interp(*this);
	ArmDisasm disasm;

	bool step_ok = true;

	printf("initialising decode cache\n");
	for (int i = 0; i < DECODE_CACHE_ENTRIES; i++) {
		ArmDecode *decode = get_decode(i);
		new (decode) ArmDecode();
		decode->pc = 0;
	}

	printf("starting cpu execution\n");

	do {
		uint32_t pc = state.regs.RB[15];

		state.last_exception_action = 0;

		ArmDecode *insn = get_decode(pc);
		if (pc == 0 || insn->pc != pc) insn->decode(ArmDecode::arm, pc);

#ifdef TRACE
		printf("[%08x] %08x %30s ", pc, insn->ir, disasm.disassemble(pc, *insn));
#endif

		step_ok = interp.step_single(*insn);
		inc_insns_executed();

		/*if ((get_insns_executed() % 10000000) == 0)
			printf("insns: %d\n", get_insns_executed());*/

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
