#include <devices/coco.h>
#include <printf.h>

using namespace captive::arch::arm::devices;

CoCo::CoCo(Environment& env) : Coprocessor(env)
{

}

CoCo::~CoCo()
{

}

bool CoCo::mcr(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data)
{
	switch (rn) {
	case 1:
		L2 = (data & (1 << 26)) != 0;
		EE = (data & (1 << 25)) != 0;
		VE = (data & (1 << 24)) != 0;
		XP = (data & (1 << 23)) != 0;
		U = (data & (1 << 22)) != 0;
		FI = (data & (1 << 21)) != 0;
		L4 = (data & (1 << 15)) != 0;
		RR = (data & (1 << 14)) != 0;
		V = (data & (1 << 13)) != 0;
		I = (data & (1 << 12)) != 0;
		Z = (data & (1 << 11)) != 0;
		F = (data & (1 << 10)) != 0;
		R = (data & (1 << 9)) != 0;
		S = (data & (1 << 8)) != 0;
		B = (data & (1 << 7)) != 0;
		L = (data & (1 << 6)) != 0;
		D = (data & (1 << 5)) != 0;
		P = (data & (1 << 4)) != 0;
		W = (data & (1 << 3)) != 0;
		C = (data & (1 << 2)) != 0;
		A = (data & (1 << 1)) != 0;
		M = (data & (1 << 0)) != 0;
		break;

	case 2:
		switch (op2) {
		case 0:
			TTBR0 = data;
			break;
		case 1:
			TTBR1 = data;
			break;
		}
		break;

	case 3:
		DACR = data;
		break;

	case 5:
		FSR = data;
		break;
	case 6:
		FAR = data;
		break;
	}

	return true;
}

bool CoCo::mrc(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data)
{
	data = 0;

	switch (rn) {
	case 0: // System Information
		switch (rm) {
		case 0: // Read CPUID
			data = 0x41069265;
			break;

		case 1: // cache type register
			//000 0111 1 00 0000 000 1 10 00 0000 000 1 10
			//0000 1111 0000 0000 0110 0000 0000 0110
			data = 0x0f006006;
			break;

		case 2: // tightly coupled memory register
			// 0000000000 0000 000 1 0000 0000 000 1 00
			// 0000 0000 0000 0000 0100 0000 0000 0100
			data = 0x00004004;
			break;
		}
		break;

	case 1:	// System Control
		data = 0;

		//NIBBLE 0
		data |= M << 0; // MMU
		data |= 0 << 1; //Strict alignment
		data |= 1 << 2; //L1 U$ or D$
		data |= 0 << 3; //Write buffer

		//NIBBLE 1
		data |= 0x7 << 4; //SBO
		data |= 0 << 7; //B (endianness)

		//NIBBLE 2
		data |= S << 8; //S (system protection)
		data |= R << 9; //R (rom protect)
		data |= 0 << 10; //F (implementation defined)
		data |= 0 << 11; //BPU

		//NIBBLE 3
		data |= 1 << 12; //I L1 I$

		data |= V << 13; //V, exception vectors

		data |= 0 << 14; //RR
		data |= 0 << 15; //L4

		//NIBBLE 4
		data |= 1 << 16; //DT
		data |= 0 << 17; //SBZ
		data |= 0 << 18; //IT //QEMU HACK
		data |= 1 << 19; //SBZ //QEMU HACK

		//NIBBLE 5
		data |= 0 << 20; //ST
		data |= 0 << 21; //FI
		data |= 1 << 22; //U
		data |= 0 << 23; //XP

		//NIBBLE 6
		data |= 0 << 24; //VE
		data |= 0 << 25; //EE
		data |= 0 << 26; //L2

		break;

	case 2:
		switch (op2) {
		case 0:
			data = TTBR0;
			break;
		case 1:
			data = TTBR1;
			break;
		}
		break;

	case 3:
		data = DACR;
		break;
	case 5:
		data = FSR;
		break;
	case 6:
		data = FAR;
		break;
	case 7:
		data = 1 << 30;
		break;
	}

	return true;
}
