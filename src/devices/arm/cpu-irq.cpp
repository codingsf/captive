#include <devices/arm/cpu-irq.h>
#include <hypervisor/cpu.h>
#include <hypervisor/guest.h>
#include <shmem.h>
#include <captive.h>

DECLARE_CONTEXT(ArmCpuIRQController);

#include <chrono>

static std::chrono::high_resolution_clock::time_point xxx;

void trigger_irq_latency_measure()
{
	std::chrono::high_resolution_clock::time_point yyy = std::chrono::high_resolution_clock::now();

	//fprintf(stderr, "LATENCY: %d ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(yyy - xxx).count());
}

using namespace captive::devices::irq;
using namespace captive::devices::arm;

void ArmCpuIRQController::irq_raised(IRQLine& line)
{
	// DEBUG << CONTEXT(ArmCpuIRQController) << "IRQ Raised: " << line.index();
	cpu().per_cpu_data().isr |= (1 << line.index());

	xxx = std::chrono::high_resolution_clock::now();

	cpu().interrupt(3);
}

void ArmCpuIRQController::irq_rescinded(IRQLine& line)
{
	//DEBUG << CONTEXT(ArmCpuIRQController) << "IRQ Rescinded: " << line.index() << ENABLE;
	cpu().per_cpu_data().isr &= ~(1 << line.index());
}
