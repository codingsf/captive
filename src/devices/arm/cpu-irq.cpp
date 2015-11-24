#include <devices/arm/cpu-irq.h>
#include <hypervisor/cpu.h>
#include <hypervisor/guest.h>
#include <shmem.h>
#include <captive.h>

DECLARE_CONTEXT(ArmCpuIRQController);

#include <chrono>

using namespace captive::devices::irq;
using namespace captive::devices::arm;

void ArmCpuIRQController::irq_raised(IRQLine& line)
{
	DEBUG << CONTEXT(ArmCpuIRQController) << "IRQ Raised: " << line.index();
	cpu().per_cpu_data().isr |= (1 << line.index());
	cpu().raise_guest_interrupt(line.index());
}

void ArmCpuIRQController::irq_rescinded(IRQLine& line)
{
	DEBUG << CONTEXT(ArmCpuIRQController) << "IRQ Rescinded: " << line.index();
	cpu().per_cpu_data().isr &= ~(1 << line.index());
	cpu().rescind_guest_interrupt(line.index());
}

void ArmCpuIRQController::irq_acknowledged(IRQLine& line)
{
	DEBUG << CONTEXT(ArmCpuIRQController) << "IRQ Acknowledged: " << line.index();
	cpu().acknowledge_guest_interrupt(line.index());
}
