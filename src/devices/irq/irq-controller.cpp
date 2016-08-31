#include <devices/irq/irq-controller.h>
#include <devices/arm/gic.h>

using namespace captive::devices::irq;

IRQControllerBase::IRQControllerBase()
{

}

IRQControllerBase::~IRQControllerBase()
{

}

void IRQControllerBase::irq_acknowledged(IRQLineBase& line)
{

}

template<typename irq_line_t, uint32_t nr_lines>
IRQController<irq_line_t, nr_lines>::IRQController()
{
	for (uint32_t i = 0; i < nr_lines; i++) {
		lines[i].attach(*this, i);
	}
}

template<typename irq_line_t, uint32_t nr_lines>
IRQController<irq_line_t, nr_lines>::~IRQController()
{

}

template<typename irq_line_t, uint32_t nr_lines>
void IRQController<irq_line_t, nr_lines>::dump() const
{
	for (int i = 0; i < nr_lines; i++) {
		fprintf(stderr, "[%02d] = %s\n", i, lines[i].raised() ? "high" : "low");
	}
}

template class IRQController<captive::devices::arm::GICIRQLine, 96u>;
template class IRQController<IRQLineBase, 32u>;
template class IRQController<IRQLineBase, 2u>;
