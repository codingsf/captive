#include <simulation/insn-count.h>
#include <hypervisor/cpu.h>

using namespace captive::simulation;

bool InstructionCounter::init()
{
	return true;
}

void InstructionCounter::start()
{

}

void InstructionCounter::stop()
{

}

void InstructionCounter::dump()
{
	fprintf(stderr, "*** INSTRUCTION COUNT *** (%lu)\n", _count);
	
	for (auto core : cores()) {
		fprintf(stderr, "core %d: %lu\n", core->id(), core->per_cpu_data().insns_executed);
	}
}

void InstructionCounter::instruction_fetch(hypervisor::CPU& core, uint32_t virt_pc, uint32_t phys_pc, uint8_t size)
{
	_count++;
}
