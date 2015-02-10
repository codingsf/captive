#include <arm-decode.h>

#define UNSIGNED_BITS(_ir, _end, _start) 0

#include "include/arm-decode.h"

using namespace captive::arch::arm;

void ArmDecode::decode(uint32_t address)
{
	opcode = UNKNOWN;
	ir = *(uint32_t *) (uint64_t) address;
}
