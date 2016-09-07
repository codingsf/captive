#include <devices/captive-coprocessor.h>
#include <printf.h>

using namespace captive::arch::arm;
using namespace captive::arch::arm::devices;

CaptiveCoprocessor::CaptiveCoprocessor(Environment& env) : Coprocessor(env, 14)
{

}

CaptiveCoprocessor::~CaptiveCoprocessor()
{

}

bool CaptiveCoprocessor::mcr(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data)
{
	printf("captive-coco: mcr rm=%d, rn=%d, op1=%d, op2=%d data=%d\n\r", rn, rm, op1, op2, data);
	
	asm volatile("out %%eax, $0xef" :: "a"((uint32_t)data));
	
	return true;
}

bool CaptiveCoprocessor::mrc(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data)
{
	printf("captive-coco: mrc rm=%d, rn=%d, op1=%d, op2=%d\n\r", rn, rm, op1, op2);
	
	data = 0;
	return true;
}
