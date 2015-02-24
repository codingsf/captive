#include <cpu.h>

using namespace captive::arch;

CPU *captive::arch::active_cpu;

CPU::CPU(Environment& env) : _env(env), insns_executed(0)
{

}

CPU::~CPU()
{

}
