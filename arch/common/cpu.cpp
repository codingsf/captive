#include <cpu.h>

using namespace captive::arch;

CPU::CPU(Environment& env) : _env(env), insns_executed(0)
{

}

CPU::~CPU()
{

}

