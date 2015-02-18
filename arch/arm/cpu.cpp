//#define TRACE

#include <arm-cpu.h>
#include <arm-env.h>
#include <arm-decode.h>
#include <arm-interp.h>
#include <arm-disasm.h>
#include <arm-mmu.h>
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
		if (pc == 0 || insn->pc != pc) {
			if (insn->decode(ArmDecode::arm, pc)) {
				printf("cpu: unhandled decode fault\n");
				return false;
			}
		}

		if (unlikely(trace)) {
			printf("%d [%08x] %08x %30s ", get_insns_executed(), pc, insn->ir, disasm.disassemble(pc, *insn));
		}

		step_ok = interp.step_single(*insn);
		inc_insns_executed();

		if (unlikely(trace)) {
			printf("\n");
		}

		if (get_insns_executed() == 95271287) trace = true;
	} while(step_ok);

	return true;
}

bool ArmCPU::init(unsigned int ep)
{
	printf("cpu init @ %x\n", ep);

	printf("creating mmu\n");
	_mmu = new ArmMMU(*this);
	_ep = ep;

	printf("installing 3-byte bootloader\n");
	volatile uint32_t *mem = (volatile uint32_t *)0;
	*mem++ = 0xef000000;
	*mem++ = 0xe1a00000;
	*mem++ = 0xe12fff1c;

	printf("clearing state\n");
	bzero(&state, sizeof(state));

	state.regs.RB[1] = 0x25e;		// Some sort of ID
	state.regs.RB[2] = 0x1000;		// Device-tree location
	state.regs.RB[12] = ep;			// Kernel entry-point
	state.regs.RB[15] = 0;			// Reset Vector

	return true;
}

void ArmCPU::handle_angel_syscall()
{
	printf("ANGEL\n");
}
