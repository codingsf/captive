#include <aarch64-cpu.h>
#include <devices/system-registers.h>
#include <printf.h>

using namespace captive::arch::aarch64::devices;

SystemRegisters::SystemRegisters(Environment& env) : CoreDevice(env), CPACR(0), MAIR(0), MDSCR(0), SCTLR(0), TCR(0), CNTPCT(0), CNTVOFF(0), VBAR(0), TPIDR_EL0(0), TPIDR_EL1(0)
{
	
}

SystemRegisters::~SystemRegisters()
{

}

#define EXTRACT_SYSREG(v) (v & 0x001fffe0)
#define HANDLE_SYSREG(op0,op1,crn,crm,op2) case (op0 << 19 | op1 << 16 | crn << 12 | crm << 8 | op2 << 5)

#define DCZID_EL0	HANDLE_SYSREG(3,3,0,0,7)
#define MPIDR_EL1	HANDLE_SYSREG(3,0,0,0,5)
#define SP_EL0		HANDLE_SYSREG(3,0,4,1,0)

#define VBAR_EL1	HANDLE_SYSREG(3,0,12,0,0)
#define VBAR_EL2	HANDLE_SYSREG(3,4,12,0,0)
#define VBAR_EL3	HANDLE_SYSREG(3,6,12,1,0)

bool SystemRegisters::read(CPU& cpu, uint32_t reg, uint64_t& data)
{
	register_access ra = decode_access(reg);
	//printf("sys-reg: read:  [%08x] op0=%d, op1=%d, rn=%d, rm=%d, op2=%d\n", reg, ra.op0, ra.op1, ra.crn, ra.crm, ra.op2);

	aarch64_cpu& a64core = (aarch64_cpu&)cpu;
	switch (EXTRACT_SYSREG(reg)) {
	HANDLE_SYSREG(3,3,0,0,1): data = 0x0444c004; return true;					// CTR_EL0
	HANDLE_SYSREG(3,0,0,7,0): data = 0x00100000; return true;					// ID_AA64MMFR0_EL1
	HANDLE_SYSREG(3,0,0,7,1): data = 0x00000000; return true;					// ID_AA64MMFR1_EL1
	HANDLE_SYSREG(3,0,1,0,0): data = SCTLR; return true;						// SCTLR
	HANDLE_SYSREG(3,0,1,0,2): data = CPACR; return true;						// CPACR
	HANDLE_SYSREG(2,0,0,2,2): data = MDSCR; return true;						// MDSCR
	HANDLE_SYSREG(3,0,0,5,0): data = 0x10101006; return true;					// AA64DFR0
	HANDLE_SYSREG(3,0,10,2,0): data = MAIR; return true;						// MAIR
	HANDLE_SYSREG(3,0,2,0,2): data = TCR; return true;							// TCR
	
	HANDLE_SYSREG(3,3,14,0,1): data = __rdtsc(); return true;
	HANDLE_SYSREG(3,4,14,0,3): data = CNTVOFF; return true;
	HANDLE_SYSREG(3,3,14,0,2): data = __rdtsc() - CNTVOFF; return true;
	
	HANDLE_SYSREG(3,3,13,0,2): data = TPIDR_EL0; return true;
	HANDLE_SYSREG(3,0,13,0,4): data = TPIDR_EL1; return true;
	
	MPIDR_EL1: data = 0x40000000; return true;
	DCZID_EL0: data = 0; return true;
	
	SP_EL0: data = a64core.reg_offsets.SP[0]; return true;
	
	default:
		printf("sys-reg: read:  [%08x] op0=%d, op1=%d, rn=%d, rm=%d, op2=%d\n", reg, ra.op0, ra.op1, ra.crn, ra.crm, ra.op2);
		for(;;);
	}
	
	data = 0;
	return false;
}

bool SystemRegisters::write(CPU& cpu, uint32_t reg, uint64_t data)
{
	register_access ra = decode_access(reg);
	printf("sys-reg: write: [%08x] op0=%d, op1=%d, rn=%d, rm=%d, op2=%d, data=%016lx\n", reg, ra.op0, ra.op1, ra.crn, ra.crm, ra.op2, data);

	aarch64_cpu& a64core = (aarch64_cpu&)cpu;
	switch (EXTRACT_SYSREG(reg)) {
	HANDLE_SYSREG(3,0,1,0,0): SCTLR = data; update_sctlr(a64core); return true;		// SCTLR
	HANDLE_SYSREG(3,0,1,0,2): CPACR = data; return true;							// CPACR
	HANDLE_SYSREG(2,0,0,2,2): MDSCR = data; return true;						// MDSCR
	HANDLE_SYSREG(3,0,10,2,0): MAIR = data; return true;						// MAIR
	HANDLE_SYSREG(3,0,2,0,2): TCR = data; return true;							// TCR
	
	HANDLE_SYSREG(3,4,14,0,3): CNTVOFF = data; return true;
	
	HANDLE_SYSREG(3,3,13,0,2): TPIDR_EL0 = data; return true;
	HANDLE_SYSREG(3,0,13,0,4): TPIDR_EL1 = data; return true;
	
	SP_EL0: a64core.reg_offsets.SP[0] = data; return true;
	
	VBAR_EL1: VBAR = data; return true;
	
	default:
		printf("sys-reg: write: [%08x] op0=%d, op1=%d, rn=%d, rm=%d, op2=%d, data=%016lx\n", reg, ra.op0, ra.op1, ra.crn, ra.crm, ra.op2, data);
		for(;;);
	}
		
	return false;
}

SystemRegisters::register_access SystemRegisters::decode_access(uint32_t ir)
{
	register_access ra;
	
	ra.crm = (ir >> 8) & 0xf;
	ra.crn = (ir >> 12) & 0xf;
	ra.op0 = (ir >> 19) & 0x3;
	ra.op1 = (ir >> 16) & 0x7;
	ra.op2 = (ir >> 5) & 0x7;
	
	return ra;
}

void SystemRegisters::update_sctlr(aarch64_cpu& cpu)
{
	if (SCTLR & 1) {
		cpu.mmu().enable();
	} else {
		cpu.mmu().disable();
	}
}
