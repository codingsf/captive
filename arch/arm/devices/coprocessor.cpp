#include <devices/coprocessor.h>
#include <printf.h>

using namespace captive::arch::arm::devices;

Coprocessor::Coprocessor(Environment& env) : Device(env), op1(0), op2(0), rn(0), rm(0)
{

}

Coprocessor::~Coprocessor()
{

}

bool Coprocessor::read(uint32_t reg, uint32_t& data)
{
	switch (reg) {
	case 0: data = op1; return true;
	case 1: data = op2; return true;
	case 2: data = rn; return true;
	case 3: data = rm; return true;
	case 4: return mrc(op1, op2, rn, rm, data);
	default: return false;
	}
}

bool Coprocessor::write(uint32_t reg, uint32_t data)
{
	switch (reg) {
	case 0: op1 = data; return true;
	case 1: op2 = data; return true;
	case 2: rn = data; return true;
	case 3: rm = data; return true;
	case 4: return mcr(op1, op2, rn, rm, data);
	default: return false;
	}
}
