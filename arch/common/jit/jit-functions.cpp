#include <define.h>
#include <printf.h>
#include <cpu.h>
#include <env.h>
#include <interp.h>

extern "C" void cpu_set_mode(void *cpu, uint8_t mode)
{
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

extern "C" void trace_reg_write(captive::arch::CPU *cpu, uint64_t off, uint32_t val)
{
	printf("TRACE: REG-WRITE: off=%u, val=0x%x\n", (uint32_t)off, val);
}

extern "C" void jit_verify(captive::arch::CPU *cpu)
{
	if (!cpu->verify_check()) {
		abort();
	}
}

extern "C" void cpu_check_interrupts(captive::arch::CPU *cpu)
{
	if (unlikely(cpu->cpu_data().isr)) {
		cpu->interpreter().handle_irq(cpu->cpu_data().isr);
	}
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
	cpu->emulate_user_mode_begin();
	*val = *((volatile uint8_t *)(uint64_t)addr);
	cpu->emulate_user_mode_end();
}

extern "C" void mem_user_read16(captive::arch::CPU *cpu, uint32_t addr, uint16_t *val)
{
	cpu->emulate_user_mode_begin();
	*val = *((volatile uint16_t *)(uint64_t)addr);
	cpu->emulate_user_mode_end();
}

extern "C" void mem_user_read32(captive::arch::CPU *cpu, uint32_t addr, uint32_t *val)
{
	cpu->emulate_user_mode_begin();
	*val = *((volatile uint32_t *)(uint64_t)addr);
	cpu->emulate_user_mode_end();
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
	cpu->emulate_user_mode_begin();
	*((volatile uint8_t *)(uint64_t)addr) = val;
	cpu->emulate_user_mode_end();
}

extern "C" void mem_user_write16(captive::arch::CPU *cpu, uint32_t addr, uint16_t val)
{
	cpu->emulate_user_mode_begin();
	*((volatile uint16_t *)(uint64_t)addr) = val;
	cpu->emulate_user_mode_end();
}

extern "C" void mem_user_write32(captive::arch::CPU *cpu, uint32_t addr, uint32_t val)
{
	cpu->emulate_user_mode_begin();
	*((volatile uint32_t *)(uint64_t)addr) = val;
	cpu->emulate_user_mode_end();
}
