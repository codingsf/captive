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

ArmCPU::ArmCPU(ArmEnvironment& env, captive::PerCPUData *per_cpu_data) : CPU(env, per_cpu_data)
{

}

ArmCPU::~ArmCPU()
{

}

void ArmCPU::enqueue_irq_check_if_enabled()
{
	if (!state.regs.I) enqueue_irq_check();
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
	_pc_reg_ptr = &state.regs.RB[15];

	jit_state.registers = &state.regs;
	jit_state.registers_size = sizeof(state.regs);

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

bool ArmCPU::decode_instruction_virt(gva_t addr, Decode* insn)
{
	return ((ArmDecode*)insn)->decode(ArmDecode::arm, addr, (uint8_t *)(uint64_t)addr);
}

bool ArmCPU::decode_instruction_phys(gpa_t addr, Decode* insn)
{
	return ((ArmDecode*)insn)->decode(ArmDecode::arm, addr, (uint8_t *)((uint64_t)addr + 0x100000000));
}

captive::arch::JumpInfo ArmCPU::get_instruction_jump_info(Decode* insn)
{
	return ArmDecode::get_jump_info((ArmDecode *)insn);
}
