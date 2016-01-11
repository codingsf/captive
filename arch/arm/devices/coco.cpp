#include <devices/coco.h>
#include <arm-cpu.h>
#include <arm-env.h>
#include <mmu.h>
#include <printf.h>

using namespace captive::arch::arm;
using namespace captive::arch::arm::devices;

//#define DEBUG_COCO

CoCo::CoCo(Environment& env) : Coprocessor(env, 15),
		_A(false), _C(false), _Z(false), _I(false), _EE(false), _TRE(false), _AFE(false), _TE(false),
		DATA_TCM_REGION(0x00000014),
		INSN_TCM_REGION(0x00000014),
		CACHE_SIZE_SELECTION(0),
		PRIMARY_REGION_REMAP(0x00098AA4),
		NORMAL_REGION_REMAP(0x44E048E0),
		CONTEXT_ID(0),
		TPID(0)
{

}

CoCo::~CoCo()
{

}

bool CoCo::mcr(CPU& cpu, uint32_t op1, uint32_t op2, uint32_t rn, uint32_t rm, uint32_t data)
{
#ifdef DEBUG_COCO
	printf("mcr p15: rn=%d, op1=%d, rm=%d, op2=%d, data=%08x\n", rn, op1, rm, op2, data);
#endif
	
	switch (rn) {
	case 0:
		switch (rm) {
		case 0:
			switch (op1) {
			case 2:
				switch (op2) {
				case 0:
					CACHE_SIZE_SELECTION = data;
					return true;
				}
				break;
			}
			break;
		}
		break;
	case 1:
	{
		if ((data & 1) && !cpu.mmu().enabled()) {
			cpu.mmu().enable();
		} else if (!(data & 1) && cpu.mmu().enabled()) {
			cpu.mmu().disable();
		}
		
		_A   = !!(data & (1 << 1));
		_C   = !!(data & (1 << 2));
		_Z   = !!(data & (1 << 11));
		_I   = !!(data & (1 << 12));
		*(((arm_cpu&)cpu).reg_offsets.cpV) = !!(data & (1 << 13));
		_EE  = !!(data & (1 << 25));
		_TRE = !!(data & (1 << 28));
		_AFE = !!(data & (1 << 29));
		_TE  = !!(data & (1 << 30));

		assert(!_AFE);
		assert(!_EE);
		assert(!_TE);
		assert(!_A);

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
		case 7:
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
		
	case 10:
		switch (rm) {
		case 2:
			switch (op1) {
			case 0:
				switch (op2) {
				case 0:
					PRIMARY_REGION_REMAP = data;
					return true;
				case 1:
					NORMAL_REGION_REMAP = data;
					return true;
				}
				break;
			}
			break;
		}
		break;
		
	case 13:
		switch (rm) {
		case 0:
			switch (op1) {
			case 0:
				switch (op2) {
				case 1:
					CONTEXT_ID = data;
					return true;
					
				case 4:
					TPID = data;
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
	enum arm_environment::core_variant core_type = ((arm_environment&)(cpu.env())).core_variant();
	
#ifdef DEBUG_COCO
	printf("mrc p15: rn=%d, op1=%d, rm=%d, op2=%d\n", rn, op1, rm, op2);
#endif
	
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
					switch (core_type) {
					case arm_environment::CortexA8:
						data = 0x410fc083;		// Cortex A8
						return true;
					case arm_environment::CortexA9:
						data = 0x414fc091;		// Cortex A9
						return true;
					}
					
					return false;

				case 1: // CACHE TYPE
					//data = 0x0f006006;		// ARMv5
					switch (core_type)
					{
					case arm_environment::CortexA8:
						data = 0x82048004;		// Cortex A8
						return true;
					case arm_environment::CortexA9:
						data = 0x83338003;		// Cortex A9
						return true;
					}
					
					return false;

				case 2: // TCM STATUS
					//data = 0x00004004;		// ARMv5
					switch (core_type)
					{
					case arm_environment::CortexA8:
					case arm_environment::CortexA9:
						data = 0;				// Cortex A8/A9
						return true;
					}
					
					return false;

				case 3:	// TLB TYPE
					switch (core_type) {
					case arm_environment::CortexA8:
						data = 0; //0x00202001;	// Cortex A8
						return true;

					case arm_environment::CortexA9:
						data = 0;				// Cortex A9
						return true;
					}
					
					return false;

				case 5:	// MP ID
					data = 0x80000000;
					return true;
				}
				break;
				
			case 1:
				switch (op2) {
				case 0: // CACHE SIZE IDENTIFICATION
					switch (CACHE_SIZE_SELECTION) {
					case 0:
						data = 0xe007e01a;
						return true;
					case 1:
						data = 0x2007e01a;
						return true;
					case 2:
						data = 0xf0000000;
						return true;
					default:
						data = 0;
						return true;
					}
					break;
					
				case 1:
					data = 0x0A000023;
					return true;
				}
				break;
				
			case 2:
				switch (op2) {
				case 0: // CACHE SIZE SELECTION
					data = CACHE_SIZE_SELECTION;
					return true;
				}
				break;
			}
			break;
			
		case 1:
			switch (op1) {
			case 0:
				switch (op2) {
				case 0:
					//data = 0x00001031;
					data = 0x00001131;
					return true;
					
				case 4:
					//data = 0x01100003;
					data = 0x31100003;
					return true;
					
				case 5:
					data = 0x20000000;
					return true;
					
				case 7:
					data = 0x00000211;
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
					data = 0x00101111;
					return true;
					
				case 3:
					data = 0x11112131;
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
		
		// M - MMU Enabled
		if (cpu.mmu().enabled()) {
			data |= 1;
		}
		
		// A - Strict Alignment
		data |= _A << 1;
		
		// C - Data Caching
		data |= _C << 2;
		
		// 6:3 - RAO, 10:7, RAZ
		data |= 0xf << 3;
		
		// Z
		data |= _Z << 11;
		
		// I
		data |= _I << 12;
		
		// V
		data |= (!!*(((arm_cpu&)cpu).reg_offsets.cpV)) << 13;
		
		// 24:14 - 0b01100010100
		data |= 0x314 << 14;
		
		// EE
		data |= _EE << 25;
		
		// TRE
		data |= _TRE << 28;
		
		// AFE
		data |= _AFE << 29;
		
		// TE
		data |= _TE << 30;
		
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
		
	case 13:
		switch (rm) {
		case 0:
			switch (op1) {
			case 0:
				switch (op2) {
				case 1:
					data = CONTEXT_ID;
					return true;
					
				case 4:
					data = TPID;
					return true;
				}
				break;
			}
			break;
		}
		break;
		
	case 15:
		switch (rm) {
		case 0:
			switch (op1) {
			case 4:
				switch (op2) {
				case 0:
					data = 0x1f000000;
					return true;
				}
				break;
			}
			break;
		}
		break;
	}

	fatal("**** unknown system control coprocessor read: rn=%d, rm=%d, op1=%d, op2=%d\n", rn, rm, op1, op2);
	return true;
}

#ifdef DEBUG_COCO
# define DEFINE_COCO_REGISTER(_crn, _op1, _crm, _op2, _name, _rw)
#else
# define DEFINE_COCO_REGISTER(_crn, _op1, _crm, _op2, _name, _rw)
#endif

DEFINE_COCO_REGISTER(0, 0, 0, 0, "Main ID", false);
DEFINE_COCO_REGISTER(0, 0, 0, 4, "Main ID", false);
DEFINE_COCO_REGISTER(0, 0, 0, 6, "Main ID", false);
DEFINE_COCO_REGISTER(0, 0, 0, 7, "Main ID", false);
DEFINE_COCO_REGISTER(0, 0, 0, 1, "Cache Type", false);
DEFINE_COCO_REGISTER(0, 0, 0, 2, "TCM Type", false);
DEFINE_COCO_REGISTER(0, 0, 0, 3, "TLB Type", false);
DEFINE_COCO_REGISTER(0, 0, 0, 5, "Multiprocessor ID", false);
DEFINE_COCO_REGISTER(0, 0, 1, 0, "Processor Feature 0", false);
DEFINE_COCO_REGISTER(0, 0, 1, 1, "Processor Feature 1", false);
DEFINE_COCO_REGISTER(0, 0, 1, 2, "Debug Feature 0", false);
DEFINE_COCO_REGISTER(0, 0, 1, 3, "Auxiliary Feature 0", false);
DEFINE_COCO_REGISTER(0, 0, 1, 4, "Memory Model Feature 0", false);
DEFINE_COCO_REGISTER(0, 0, 1, 5, "Memory Model Feature 1", false);
DEFINE_COCO_REGISTER(0, 0, 1, 6, "Memory Model Feature 2", false);
DEFINE_COCO_REGISTER(0, 0, 1, 7, "Memory Model Feature 3", false);
DEFINE_COCO_REGISTER(0, 0, 2, 0, "Instruction Set Attribute 0", false);
DEFINE_COCO_REGISTER(0, 0, 2, 1, "Instruction Set Attribute 1", false);
DEFINE_COCO_REGISTER(0, 0, 2, 2, "Instruction Set Attribute 2", false);
DEFINE_COCO_REGISTER(0, 0, 2, 3, "Instruction Set Attribute 3", false);
DEFINE_COCO_REGISTER(0, 0, 2, 4, "Instruction Set Attribute 4", false);
DEFINE_COCO_REGISTER(0, 0, 2, 5, "Instruction Set Attribute 5", false);
DEFINE_COCO_REGISTER(0, 0, 2, 6, "Instruction Set Attribute 6", false);
DEFINE_COCO_REGISTER(0, 0, 2, 7, "Instruction Set Attribute 7", false);

DEFINE_COCO_REGISTER(0, 0, 3, 0, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 3, 1, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 3, 2, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 3, 3, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 3, 4, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 3, 5, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 3, 6, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 3, 7, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 4, 0, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 4, 1, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 4, 2, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 4, 3, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 4, 4, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 4, 5, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 4, 6, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 4, 7, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 5, 0, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 5, 1, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 5, 2, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 5, 3, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 5, 4, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 5, 5, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 5, 6, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 5, 7, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 6, 0, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 6, 1, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 6, 2, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 6, 3, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 6, 4, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 6, 5, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 6, 6, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 6, 7, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 7, 0, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 7, 1, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 7, 2, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 7, 3, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 7, 4, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 7, 5, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 7, 6, "Reserved Feature ID", false);
DEFINE_COCO_REGISTER(0, 0, 7, 7, "Reserved Feature ID", false);
