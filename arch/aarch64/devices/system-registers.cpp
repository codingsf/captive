#include <devices/system-registers.h>
#include <printf.h>

using namespace captive::arch::aarch64::devices;

SystemRegisters::SystemRegisters(Environment& env) : CoreDevice(env)
{
	
}

SystemRegisters::~SystemRegisters()
{

}

bool SystemRegisters::read(CPU& cpu, uint32_t reg, uint32_t& data)
{
	register_access ra = decode_access(reg);
	
	switch (ra.op0) {
	case 3:
		switch (ra.op1) {
		case 3:
			switch (ra.crn) {
			case 0:
				switch (ra.crm) {
				case 0:
					switch (ra.op2) {
					case 1:					// CTR_EL0
						data = 0x0444c004;
						return true;
					}
					break;
				}
				break;
			}
			break;
		}
		break;
	}

	printf("unhandled sys-reg: read: op0=%d, op1=%d, op2=%d, rm=%d, rn=%d\n", ra.op0, ra.op1, ra.op2, ra.crm, ra.crn);
	
	data = 0;
	return false;
}

bool SystemRegisters::write(CPU& cpu, uint32_t reg, uint32_t data)
{
	register_access ra = decode_access(reg);

	printf("sys-reg: write: op0=%d, op1=%d, op2=%d, rm=%d, rn=%d, data=%08x\n", ra.op0, ra.op1, ra.op2, ra.crm, ra.crn, data);
	
	return true;
}

SystemRegisters::register_access SystemRegisters::decode_access(uint32_t ir)
{
	register_access ra;
	
	ra.crm = (ir >> 8) & 0xf;
	ra.crn = (ir >> 12) & 0xf;
	ra.op0 = (ir >> 19) & 0x3;
	ra.op1 = (ir >> 16) & 0x7;
	ra.op2 = (ir >> 5) & 0x7;
	
	return ra;
}
