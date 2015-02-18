#include <devices/irq/irq-controller.h>
#include <devices/irq/irq-line.h>

using namespace captive::devices::irq;

IRQLine::IRQLine() : _raised(false), _index(0)
{

}


void IRQLine::raise()
{
	if (!_raised) {
		_raised = true;
		_controller->irq_raised(*this);
	}
}

void IRQLine::rescind()
{
	if (_raised) {
		_raised = false;
		_controller->irq_rescinded(*this);
	}
}
