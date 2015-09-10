#include <define.h>
#include <printf.h>
#include <cpu.h>
#include <env.h>
#include <priv.h>

extern "C" void cpu_set_mode(void *cpu, uint8_t mode)
{
	printf("set mode\n");
}

extern "C" void cpu_trap(void *cpu)
{
	assert(false);
}

extern "C" void cpu_write_device(captive::arch::CPU *cpu, uint32_t devid, uint32_t reg, uint32_t val)
{
	cpu->env().write_core_device(*cpu, devid, reg, val);
}

extern "C" void cpu_read_device(captive::arch::CPU *cpu, uint32_t devid, uint32_t reg, uint32_t& val)
{
	cpu->env().read_core_device(*cpu, devid, reg, val);
}

extern "C" void jit_trace(captive::arch::CPU *cpu, uint8_t opcode, uint64_t a1, uint64_t a2, uint64_t a3)
{
	switch (opcode) {
	case 0:	// START
		printf("[%08x] ", cpu->read_pc());
		break;
	case 1:	// STOP
		printf("\n");
		break;
		
	case 2: // READ MEM
		printf("M[%08x] => %08x ", a1, a2);
		break;
	case 3: // WRITE MEM
		printf("M[%08x] <= %08x ", a1, a2);
		break;

	case 4: // READ REG
		printf("R[%03d] => %08x ", a1, a2);
		break;
	case 5: // WRITE REG
		printf("R[%03d] <= %08x ", a1, a2);
		break;
		
	case 6: // READ DEV
		printf("D[%02d @ %02d] => %08x ", a1, a2, a3);
		break;
	case 7: // WRITE DEV
		printf("D[%02d @ %02d] <= %08x ", a1, a2, a3);
		break;
	}
}

extern "C" void jit_verify(captive::arch::CPU *cpu)
{
	if (!cpu->verify_check()) {
		fatal("verification failed\n");
	}
}

extern "C" void cpu_check_interrupts(captive::arch::CPU *cpu)
{
	if (unlikely(cpu->cpu_data().isr)) {
		cpu->handle_irq(cpu->cpu_data().isr);
	}
}

extern "C" void jit_rum(captive::arch::CPU *cpu)
{
	if (in_kernel_mode()) {
		printf("*** read-user-memory: in kernel mode\n");
	} else {
		printf("*** read-user-memory: in user mode\n");
	}
}

extern "C" uint32_t jit_handle_interrupt(captive::arch::CPU *cpu, uint32_t isr)
{
	return cpu->handle_irq(isr);
}

extern "C" void jit_debug1(uint32_t pc)
{
	printf("CHAIN: %x\n", pc);
}

extern "C" void jit_debug2(uint32_t pc)
{
	printf("DO DISPATCH: %x\n", pc);
}

extern "C" void mem_read8(captive::arch::CPU *cpu, uint32_t addr, uint8_t *val)
{
	*val = *((volatile uint8_t *)(uint64_t)addr);
}

extern "C" void mem_read16(captive::arch::CPU *cpu, uint32_t addr, uint16_t *val)
{
	*val = *((volatile uint16_t *)(uint64_t)addr);
}

extern "C" void mem_read32(captive::arch::CPU *cpu, uint32_t addr, uint32_t *val)
{
	*val = *((volatile uint32_t *)(uint64_t)addr);
}

extern "C" void mem_user_read8(captive::arch::CPU *cpu, uint32_t addr, uint8_t *val)
{
	if (cpu->kernel_mode()) { switch_to_user_mode(); }
	*val = *((volatile uint8_t *)(uint64_t)addr);
	if (cpu->kernel_mode() && !in_kernel_mode()) { switch_to_kernel_mode(); }
}

extern "C" void mem_user_read16(captive::arch::CPU *cpu, uint32_t addr, uint16_t *val)
{
	if (cpu->kernel_mode()) { switch_to_user_mode(); }
	*val = *((volatile uint16_t *)(uint64_t)addr);
	if (cpu->kernel_mode() && !in_kernel_mode()) { switch_to_kernel_mode(); }
}

extern "C" void mem_user_read32(captive::arch::CPU *cpu, uint32_t addr, uint32_t *val)
{
	if (cpu->kernel_mode()) { switch_to_user_mode(); }
	*val = *((volatile uint32_t *)(uint64_t)addr);
	if (cpu->kernel_mode() && !in_kernel_mode()) { switch_to_kernel_mode(); }
}

extern "C" void mem_write8(captive::arch::CPU *cpu, uint32_t addr, uint8_t val)
{
	*((volatile uint8_t *)(uint64_t)addr) = val;
}

extern "C" void mem_write16(captive::arch::CPU *cpu, uint32_t addr, uint16_t val)
{
	*((volatile uint16_t *)(uint64_t)addr) = val;
}

extern "C" void mem_write32(captive::arch::CPU *cpu, uint32_t addr, uint32_t val)
{
	*((volatile uint32_t *)(uint64_t)addr) = val;
}

extern "C" void mem_user_write8(captive::arch::CPU *cpu, uint32_t addr, uint8_t val)
{
	if (cpu->kernel_mode()) { switch_to_user_mode(); }
	*((volatile uint8_t *)(uint64_t)addr) = val;
	if (cpu->kernel_mode() && !in_kernel_mode()) { switch_to_kernel_mode(); }
}

extern "C" void mem_user_write16(captive::arch::CPU *cpu, uint32_t addr, uint16_t val)
{
	if (cpu->kernel_mode()) { switch_to_user_mode(); }
	*((volatile uint16_t *)(uint64_t)addr) = val;
	if (cpu->kernel_mode() && !in_kernel_mode()) { switch_to_kernel_mode(); }
}

extern "C" void mem_user_write32(captive::arch::CPU *cpu, uint32_t addr, uint32_t val)
{
	if (cpu->kernel_mode()) { switch_to_user_mode(); }
	*((volatile uint32_t *)(uint64_t)addr) = val;
	if (cpu->kernel_mode() && !in_kernel_mode()) { switch_to_kernel_mode(); }
}
