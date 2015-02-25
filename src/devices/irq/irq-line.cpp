#include <devices/irq/irq-controller.h>
#include <devices/irq/irq-line.h>

using namespace captive::devices::irq;

IRQLine::IRQLine() : _raised(false), _index(0)
{

}


void IRQLine::raise()
{
	bool is_raised = false;
	if (_raised.compare_exchange_strong(is_raised, true)) {
		_controller->irq_raised(*this);
	}
}

void IRQLine::rescind()
{
	bool is_raised = true;
	if (_raised.compare_exchange_strong(is_raised, false)) {
		_controller->irq_rescinded(*this);
	}
}
