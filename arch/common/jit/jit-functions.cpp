#include <define.h>
#include <printf.h>
#include <cpu.h>
#include <env.h>

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