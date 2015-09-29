#include <devices/coprocessor.h>
#include <printf.h>

using namespace captive::arch::arm::devices;

//#define DEBUG_COPROCESSOR

Coprocessor::Coprocessor(Environment& env, uint32_t id) : CoreDevice(env), op1(0), op2(0), rn(0), rm(0), _id(id)
{

}

Coprocessor::~Coprocessor()
{

}

bool Coprocessor::read(CPU& cpu, uint32_t reg, uint32_t& data)
{
	switch (reg) {
	case 0: data = op1; return true;
	case 1: data = op2; return true;
	case 2: data = rn; return true;
	case 3: data = rm; return true;
#ifdef DEBUG_COPROCESSOR
	case 4: {
		bool result = mrc(cpu, op1, op2, rn, rm, data);
		printf("mrc p%d, %d, (%08x), cr%d, cr%d, %d\n", _id, op1, data, rn, rm, op2);
		return result;
	}
#else
	case 4: return mrc(cpu, op1, op2, rn, rm, data);
#endif
	default: return false;
	}
}

bool Coprocessor::write(CPU& cpu, uint32_t reg, uint32_t data)
{
	switch (reg) {
	case 0: op1 = data; return true;
	case 1: op2 = data; return true;
	case 2: rn = data; return true;
	case 3: rm = data; return true;
#ifdef DEBUG_COPROCESSOR
	case 4: {
		printf("mcr p%d, %d, (%08x), cr%d, cr%d, %d\n", _id, op1, data, rn, rm, op2);
		return mcr(cpu, op1, op2, rn, rm, data);
	}
#else
	case 4: return mcr(cpu, op1, op2, rn, rm, data);
#endif
	default: return false;
	}
}
