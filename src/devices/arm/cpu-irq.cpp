#include <devices/arm/cpu-irq.h>

using namespace captive::devices::irq;
using namespace captive::devices::arm;

void ArmCpuIRQController::irq_raised(IRQLine& line)
{
	printf("cpu: irq raised: %d\n", line.index());
}

void ArmCpuIRQController::irq_rescinded(IRQLine& line)
{
	printf("cpu: irq rescinded: %d\n", line.index());
}
