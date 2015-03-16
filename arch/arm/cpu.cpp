//#define TRACE

#include <arm-cpu.h>
#include <arm-env.h>
#include <arm-decode.h>
#include <arm-interp.h>
#include <arm-jit.h>
#include <arm-disasm.h>
#include <arm-mmu.h>

#include <printf.h>
#include <string.h>

using namespace captive::arch::arm;

ArmCPU::ArmCPU(ArmEnvironment& env, profile::Image& profile_image, captive::PerCPUData *per_cpu_data) : CPU(env, profile_image, per_cpu_data), state(*(struct cpu_state *)0x211000000)
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
	printf("  C = %d\n", state.regs.C);
	printf("  V = %d\n", state.regs.V);
	printf("  Z = %d\n", state.regs.Z);
	printf("  N = %d\n", state.regs.N);
	printf("  M = %d\n", state.regs.M);
	printf("  F = %d\n", state.regs.F);
	printf("  I = %d\n", state.regs.I);
}

bool ArmCPU::init()
{
	//printf("cpu init @ %x\n", ep);

	_trace = new Trace(*new ArmDisasm());
	_interp = new ArmInterp(*this);
	_jit = new ArmJIT();
	_mmu = new ArmMMU(*this);

	//printf("installing 3-byte bootloader\n");
	volatile uint32_t *mem = (volatile uint32_t *)0;
	*mem++ = 0xef000000;
	*mem++ = 0xe1a00000;
	*mem++ = 0xe12fff1c;

	//printf("clearing state\n");
	bzero(&state, sizeof(state));

	state.regs.RB[1] = 0x25e;			// Some sort of ID
	state.regs.RB[2] = 0x1000;			// Device-tree location
	state.regs.RB[12] = cpu_data().entrypoint;	// Kernel entry-point
	state.regs.RB[15] = 0;				// Reset Vector

	return true;
}

bool ArmCPU::decode_instruction(uint32_t addr, Decode* insn)
{
	return ((ArmDecode*)insn)->decode(ArmDecode::arm, addr);
}
