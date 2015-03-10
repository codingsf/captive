#include <devices/arm/cpu-irq.h>
#include <hypervisor/cpu.h>
#include <hypervisor/guest.h>
#include <shmem.h>
#include <captive.h>

using namespace captive::devices::irq;
using namespace captive::devices::arm;

void ArmCpuIRQController::irq_raised(IRQLine& line)
{
	// DEBUG << CONTEXT(ArmCpuIRQController) << "IRQ Raised: " << line.index();
	cpu().per_cpu_data().isr |= (1 << line.index());
}

void ArmCpuIRQController::irq_rescinded(IRQLine& line)
{
	//DEBUG << CONTEXT(ArmCpuIRQController) << "IRQ Rescinded: " << line.index();
	cpu().per_cpu_data().isr &= ~(1 << line.index());
}
