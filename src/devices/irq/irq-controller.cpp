#include <devices/irq/irq-controller.h>

using namespace captive::devices::irq;

IRQControllerBase::IRQControllerBase()
{

}

IRQControllerBase::~IRQControllerBase()
{

}

void IRQControllerBase::irq_raised(IRQLine& line)
{

}

void IRQControllerBase::irq_rescinded(IRQLine& line)
{

}

template<uint32_t nr_lines>
IRQController<nr_lines>::IRQController()
{
	for (uint32_t i = 0; i < nr_lines; i++) {
		lines[i].attach(*this, i);
	}
}

template<uint32_t nr_lines>
IRQController<nr_lines>::~IRQController()
{

}

template class IRQController<32u>;
template class IRQController<2u>;
