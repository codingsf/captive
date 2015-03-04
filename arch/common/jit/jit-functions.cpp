#include <define.h>
#include <cpu.h>

using namespace captive::arch;

extern "C" void cpu_take_exception(CPU *cpu, uint32_t code, uint32_t data)
{
	//cpu->interpreter().
}
