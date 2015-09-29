#include <devices/debug-coprocessor.h>
#include <printf.h>

using namespace captive::arch::arm;
using namespace captive::arch::arm::devices;

DebugCoprocessor::DebugCoprocessor(Environment& env) : Coprocessor(env, 14)
{

}

DebugCoprocessor::~DebugCoprocessor()
{

}

bool DebugCoprocessor::mcr(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data)
{
	return true;
}

bool DebugCoprocessor::mrc(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data)
{
	data = 0;
	return true;
}
