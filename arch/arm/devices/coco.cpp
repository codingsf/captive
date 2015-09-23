#include <devices/coco.h>
#include <arm-cpu.h>
#include <mmu.h>
#include <printf.h>

using namespace captive::arch::arm;
using namespace captive::arch::arm::devices;

CoCo::CoCo(Environment& env) : Coprocessor(env), _R(false), _S(false), M(false), DATA_TCM_REGION(0x00000014), INSN_TCM_REGION(0x00000014)
{

}

CoCo::~CoCo()
{

}

bool CoCo::mcr(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data)
{
	switch (rn) {
	case 1:
	{
		L2 = (data & (1 << 26)) != 0;
		EE = (data & (1 << 25)) != 0;
		VE = (data & (1 << 24)) != 0;
		XP = (data & (1 << 23)) != 0;
		U = (data & (1 << 22)) != 0;
		FI = (data & (1 << 21)) != 0;
		L4 = (data & (1 << 15)) != 0;
		RR = (data & (1 << 14)) != 0;
		*(((arm_cpu&)cpu).reg_offsets.cpV) = !!(data & (1 << 13));
		I = (data & (1 << 12)) != 0;
		Z = (data & (1 << 11)) != 0;
		F = (data & (1 << 10)) != 0;
		_R = (data & (1 << 9)) != 0;
		_S = (data & (1 << 8)) != 0;
		B = (data & (1 << 7)) != 0;
		L = (data & (1 << 6)) != 0;
		D = (data & (1 << 5)) != 0;
		P = (data & (1 << 4)) != 0;
		W = (data & (1 << 3)) != 0;
		C = (data & (1 << 2)) != 0;
		A = (data & (1 << 1)) != 0;
		M = (data & (1 << 0)) != 0;

		if (M) {
			cpu.mmu().enable();
		} else {
			cpu.mmu().disable();
		}

		return true;
	}
	
	case 7:
		cpu.mmu().invalidate_virtual_mappings();
		switch (rm) {
			case 0:
				switch (op2) {
				case 4:		// WFI
					printf("WFI\n");
					return true;
				}
				break;
				
			case 5:
				switch (op2) {
				case 0:		// Invalidate entire I$
					return true;
					
				case 1:		// Invalidate I$ LINE	(MVA)
					return true;
					
				case 2:		// Invalidate I$ LINE	(SET/WAY)
					return true;
					
				case 4:		// Flush prefetch buffer
					return true;
					
				case 6:		// Flush BT$
					return true;
					
				case 7:		// Flush BT$ entry
					return true;
				}
				break;
				
			case 6:
				switch (op2) {
				case 0:		// Invalidate entire D$
					return true;
				case 1:		// Invalidate D$ LINE	(MVA)
					return true;
				case 2:		// Invalidate D$ LINE	(SET/WAY)
					return true;
				}
				break;
				
			case 7:
				switch (op2) {
				case 0:		// Invalidate unified $
					return true;
				case 1:		// Invalidate unified $ line	(MVA)
					return true;
				case 2:		// Invalidate unified $ line	(SET/WAY)
					return true;
				}
				break;
				
			case 10:
				switch (op2) {
				case 0:		// Clean entire D$
					return true;
				case 1:		// Clean D$ line	(MVA)
					return true;
				case 2:		// Clean D$ line	(SET/WAY)
					return true;
				case 3:		// Test and Clean
					return true;
				case 4:		// Data Sync Barrier
					return true;
				case 5:		// Data Mem Barrier
					return true;
				}
				break;
				
			case 11:
				switch (op2) {
				case 0:		// Clean entire unified $
					return true;
				case 1:		// Clean unified $ line		(MVA)
					return true;
				case 2:		// Clean unified $			(SET/WAY)
					return true;
				}
				break;
				
			case 13:
				switch (op2) {
				case 1:		// Prefetch I$ line		(MVA)
					return true;
				}
				break;
				
			case 14:
				switch (op2) {
				case 0:		// Clean and Invalidate entire D$
					return true;
				case 1:		// Clean and Invalidate D$ line		(MVA)
					return true;
				case 2:		// Clean and Invalidate D$ line		(SET/WAY)
					return true;
				case 3:		// Test, clean and invalidate
					return true;
				}
				break;
				
			case 15:
				switch (op2) {
				case 0:		// Clean and Invalidate entire unified $
					return true;
				case 1:		// Clean and Invalidate unified $ line	(MVA)
					return true;
				case 2:		// Clean and Invalidate unified $ line	(SET/WAY)
					return true;
				}
				break;
		}
		break;
		
	case 8:
		switch (rm) {
		case 5:
		case 6:
			cpu.mmu().invalidate_virtual_mappings();
			return true;
		}
		break;
		
	case 9:
		switch (rm) {
		case 1:
			switch (op1) {
			case 0:
				switch (op2) {
				case 0:
					DATA_TCM_REGION = data;
					return true;
				case 1:
					INSN_TCM_REGION = data;
					return true;
				}
				break;
			}
			break;
			
		case 2:
			switch (op1) {
			case 0:
				switch (op2) {
				case 0:
					return true;
				}
				break;
			}
			break;
		}
		break;
	}
	
	printf("**** unknown system control coprocessor write: rn=%d, rm=%d, op1=%d, op2=%d, data=%x\n", rn, rm, op1, op2, data);
	return true;
}

bool CoCo::mrc(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t& data)
{
	data = 0;
	
	switch (rn) {
	case 0: // System Information
		switch (rm) {
		case 0:
			switch (op1) {
			case 0:
				switch (op2) {
				case 0: // MAIN ID
					//data = 0x41069265;		// ARMv5
					data = 0x410fb767;		// ARM1176JZF-S
					return true;

				case 1: // CACHE TYPE
					//data = 0x0f006006;	// ARMv5
					data = 0x10152152;	// ARM1176JZF-S
					return true;

				case 2: // TCM STATUS
					//data = 0x00004004; // ARMv5
					data = 0x00020002;	// ARM1176JZF-S
					return true;

				case 3:	// TLB TYPE
					data = 0x00000800;	// ARM1176JZF-S
					return true;
				}
				break;
			}
			break;
			
		case 1:
			switch (op1) {
			case 0:
				switch (op2) {
				case 4:
					data = 0x01130003;
					return true;
					
				case 5:
					data = 0x10030302;
					return true;
				}
				break;
			}
			break;
			
		case 2:
			switch (op1) {
			case 0:
				switch (op2) {
				case 0:
					data = 0x00140011;
					return true;
					
				case 3:
					data = 0x01102131;
					return true;
					
				case 5:
					data = 0x00000000;
					return true;
				}
				break;
			}
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
		data |= _S << 8; //S (system protection)
		data |= _R << 9; //R (rom protect)
		data |= 0 << 10; //F (implementation defined)
		data |= 0 << 11; //BPU

		//NIBBLE 3
		data |= 1 << 12; //I L1 I$

		data |= (!!(*(((arm_cpu&)cpu).reg_offsets.cpV)) << 13); //V, exception vectors

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

		return true;
		
	case 7:
		if (rm == 14) data = 1 << 30;
		return true;
		
	case 9:
		switch (rm) {
		case 1:
			switch (op1) {
			case 0:
				switch (op2) {
				case 0:
					data = DATA_TCM_REGION;
					return true;
				case 1:
					data = INSN_TCM_REGION;
					return true;
				}
				break;
			}
			break;
		}
		break;
	}

	printf("**** unknown system control coprocessor read: rn=%d, rm=%d, op1=%d, op2=%d\n", rn, rm, op1, op2);
	return true;
}
