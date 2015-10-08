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
	printf("debug-coco: mcr rm=%d, rn=%d, op1=%d, op2=%d data=%d\n", rn, rm, op1, op2, data);
	return true;
}

bool DebugCoprocessor::mrc(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data)
{
	printf("debug-coco: mrc rm=%d, rn=%d, op1=%d, op2=%d\n", rn, rm, op1, op2, data);
	
	if (rn == 0 && rm == 0 && op1 == 0 && op2 == 0) {
		data = 0x00040003;
	} else {
		data = 0;
	}
	
	return true;
}
