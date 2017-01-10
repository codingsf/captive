#include <aarch64-env.h>
#include <aarch64-cpu.h>
#include <devices/system-registers.h>

#include <printf.h>

using namespace captive;
using namespace captive::arch;
using namespace captive::arch::aarch64;
using namespace captive::arch::aarch64::devices;

Environment *create_environment_aarch64(PerGuestData *per_guest_data)
{
	return new aarch64_environment(aarch64_environment::CortexA72, per_guest_data);
}

aarch64_environment::aarch64_environment(enum core_variant _core_variant, PerGuestData *per_guest_data) : Environment(per_guest_data), _core_variant(_core_variant)
{
	install_core_device(16, new SystemRegisters(*this));
}

aarch64_environment::~aarch64_environment()
{

}

CPU *aarch64_environment::create_cpu(PerCPUData *per_cpu_data)
{
	return new aarch64_cpu(*this, per_cpu_data);
}

bool aarch64_environment::prepare_boot_cpu(CPU* core)
{
	write_pc(core->cpu_data().guest_data->entrypoint);
	return true;
}

bool aarch64_environment::prepare_bootloader()
{
	return true;
}
