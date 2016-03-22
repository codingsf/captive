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

void IRQControllerBase::irq_acknowledged(IRQLine& line)
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

template<uint32_t nr_lines>
bool IRQController<nr_lines>::have_raised_irqs() const
{
	for (uint32_t i = 0; i < nr_lines; i++) {
		if (lines[i].raised()) {
			return true;
		}
	}

	return false;
}

template<uint32_t nr_lines>
void IRQController<nr_lines>::dump() const
{
	for (int i = 0; i < nr_lines; i++) {
		fprintf(stderr, "[%02d] = %s\n", i, lines[i].raised() ? "high" : "low");
	}
}

template class IRQController<96u>;
template class IRQController<32u>;
template class IRQController<2u>;
