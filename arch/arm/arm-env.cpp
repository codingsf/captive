#include <arm-env.h>
#include <arm-cpu.h>
#include <devices/coco.h>
#include <devices/debug-coprocessor.h>

#include <printf.h>

using namespace captive;
using namespace captive::arch;
using namespace captive::arch::arm;

Environment *create_environment_arm(PerGuestData *per_guest_data)
{
	return new arm_environment(arm_environment::ARMv7, arm_environment::CortexA8, per_guest_data);
}

arm_environment::arm_environment(enum arch_variant _arch_variant, enum core_variant _core_variant, PerGuestData *per_guest_data) : Environment(per_guest_data), _arch_variant(_arch_variant), _core_variant(_core_variant)
{
	// TODO: Abstract into platform-specific setup
	install_core_device(14, new devices::DebugCoprocessor(*this));
	install_core_device(15, new devices::CoCo(*this));
}

arm_environment::~arm_environment()
{

}

CPU *arm_environment::create_cpu(PerCPUData *per_cpu_data)
{
	return new arm_cpu(*this, per_cpu_data);
}

bool arm_environment::prepare_boot_cpu(CPU* core)
{
	arm_cpu *arm_core = (arm_cpu *)core;

	if (_core_variant == CortexA8)	
		arm_core->reg_offsets.RB[1]  = 0x769;	// Machine ID Cortex A8
	else if (_core_variant == CortexA9)
		arm_core->reg_offsets.RB[1]  = 0x76d;	// Machine ID Cortex A9
	else
		return false;
	
	arm_core->reg_offsets.RB[2]  = 0x100;	// Device Tree / ATAGs
	arm_core->reg_offsets.RB[12] = per_guest_data->entrypoint;		// Kernel Entry Point
	write_pc(0);
	
	return true;
}

bool arm_environment::prepare_bootloader()
{
	volatile uint32_t *mem = (volatile uint32_t *)0;
	*mem++ = 0xef000000;		// swi 0
	*mem++ = 0xe1a00000;		// nop
	*mem++ = 0xe12fff1c;		// bx ip

	return true;
}
